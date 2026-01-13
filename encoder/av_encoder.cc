#include "av_encoder.h"

#include <limits>
#include <string>

#include "third_party/openh264/src/codec/api/svc/codec_api.h"
#include "third_party/openh264/src/codec/api/svc/codec_app_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_def.h"
#include "third_party/openh264/src/codec/api/svc/codec_ver.h"
#include "absl/strings/match.h"
#include "common_video/h264/h264_common.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/video_coding/utility/simulcast_rate_allocator.h"
#include "modules/video_coding/utility/simulcast_utility.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/metrics.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/scale.h"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"
#include "cjson.h"
#include "cfg.h"
#include "av_api.h"
#include "../av_cfg.h"

#include "../video/video_data_recv.h"

#define  ENCODE_MEMORY_OPT 1   // avoid to copy data too many times

#define  DJI_MAX_BPS   15000

namespace webrtc {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace {

const bool kOpenH264EncoderDetailedLogging = false;

// QP scaling thresholds.
static const int kLowH264QpThreshold = 24;
static const int kHighH264QpThreshold = 37;

// Used by histograms. Values of entries should not be changed.
enum NvVideoEncoderEvent
{
	kH264EncoderEventInit = 0,
	kH264EncoderEventError = 1,
	kH264EncoderEventMax = 16,
};

VideoFrameType ConvertToVideoFrameType(EVideoFrameType type) {
	switch (type) {
	case videoFrameTypeIDR:
		return VideoFrameType::kVideoFrameKey;
	case videoFrameTypeSkip:
	case videoFrameTypeI:
	case videoFrameTypeP:
	case videoFrameTypeIPMixed:
		return VideoFrameType::kVideoFrameDelta;
	case videoFrameTypeInvalid:
		break;
	}
	RTC_NOTREACHED() << "Unexpected/invalid frame type: " << type;
	return VideoFrameType::kEmptyFrame;
}

}  // namespace
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void RtpFragmentize(EncodedImage* encoded_image,
                           const VideoFrameBuffer& frame_buffer,
                           char *data_buf, int data_size,
                           RTPFragmentationHeader* frag_header) 
{
	encoded_image->set_size(0);

	// TODO(nisse): Use a cache or buffer pool to avoid allocation?
	encoded_image->SetEncodedData(EncodedImageBuffer::Create(data_size));

	memcpy(encoded_image->data(), data_buf, data_size);

	std::vector<webrtc::H264::NaluIndex> nalus = webrtc::H264::FindNaluIndices(encoded_image->data(), encoded_image->size());

	size_t fragments_count = nalus.size();

	frag_header->VerifyAndAllocateFragmentationHeader(fragments_count);

	for (size_t i = 0; i < nalus.size(); i++) 
	{
		frag_header->fragmentationOffset[i] = nalus[i].payload_start_offset;
		frag_header->fragmentationLength[i] = nalus[i].payload_size;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
AvEncoder::AvEncoder(const cricket::VideoCodec& codec)
    : packetization_mode_(H264PacketizationMode::SingleNalUnit),
      max_payload_size_(0),
      number_of_cores_(0),
      encoded_image_callback_(nullptr),
      has_reported_init_(false),
      has_reported_error_(false),
      num_temporal_layers_(1),
      tl0sync_limit_(0) 
{
	video_format_ = EVideoFormatType::videoFormatI420; // shangtao add for tscancode
	
	//RTC_CHECK(absl::EqualsIgnoreCase(codec.name, cricket::kH264CodecName));
	std::string packetization_mode_string;
	if (codec.GetParam(cricket::kH264FmtpPacketizationMode, &packetization_mode_string) 
		&& packetization_mode_string == "1") {
		packetization_mode_ = H264PacketizationMode::NonInterleaved;
	}

	encoded_images_.reserve(kMaxSimulcastStreams);
	nv_encoders_.reserve(kMaxSimulcastStreams);
	configurations_.reserve(kMaxSimulcastStreams);
	image_buffer_ = nullptr;
	
	encoded_data_buf_ = (char *)malloc(4 * 1024 *1024);
	if(!encoded_data_buf_)
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "malloc %d failed for encoder ?", 4 * 1024 *1024);
	}
}

AvEncoder::~AvEncoder() 
{
	Release();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t AvEncoder::InitEncode(const webrtc::VideoCodec* inst, const VideoEncoder::Settings& settings) 
{
    //init_encode_event_.Set();

	ReportInit();
	if (!inst) 
	{
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	if (inst->codecType != kVideoCodecH264) 
	{
		ReportError();
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}

	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "encoder InitEncode: maxFramerate=%d width=%d height=%d keyFrameInterval=%d maxBitrate=%dkbps minBitrate=%dkbps..... ", \
				inst->maxFramerate, inst->width, inst->height, inst->H264().keyFrameInterval, inst->maxBitrate, inst->minBitrate);
	
	if (inst->maxFramerate == 0) {
		ReportError();
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "warning: av_encoder maxFramerate is 0 !!" );		
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}
	
	if (inst->width < 1 || inst->height < 1) {
		ReportError();
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "warning: av_encoder w/h erorr!!" );
		return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
	}

	int number_of_streams = SimulcastUtility::NumberOfSimulcastStreams(*inst);
	bool doing_simulcast = (number_of_streams > 1);

	if (doing_simulcast && !SimulcastUtility::ValidSimulcastParameters(*inst, number_of_streams)) {
		return WEBRTC_VIDEO_CODEC_ERR_SIMULCAST_PARAMETERS_NOT_SUPPORTED;
	}

	assert(number_of_streams == 1);

	encoded_images_.resize(number_of_streams);
	nv_encoders_.resize(number_of_streams);
	configurations_.resize(number_of_streams);

	codec_          = *inst;
	codec_settings_ = *inst;

	// Code expects simulcastStream resolutions to be correct, make sure they are
	// filled even when there are no simulcast layers.
	if (codec_.numberOfSimulcastStreams == 0) {
		codec_.simulcastStream[0].width  = codec_.width;
		codec_.simulcastStream[0].height = codec_.height;
	}

	num_temporal_layers_ = codec_.H264()->numberOfTemporalLayers;

	for (int i = 0, idx = number_of_streams - 1; i < number_of_streams;  ++i, --idx) {
		// Store nvidia encoder. In fact , this encoder is not used to do any real work !
		xop::NvidiaD3D11Encoder* nv_encoder = new xop::NvidiaD3D11Encoder();
		nv_encoders_[i] = nv_encoder;

		// Set internal settings from codec_settings
		configurations_[i].simulcast_idx = idx;
		configurations_[i].sending = false;
		configurations_[i].width = codec_.simulcastStream[idx].width;
		configurations_[i].height = codec_.simulcastStream[idx].height;
#ifndef MESH_DJI
		configurations_[i].max_frame_rate = static_cast<float>(codec_.maxFramerate);
		configurations_[i].frame_dropping_on = codec_.H264()->frameDroppingOn;
		configurations_[i].key_frame_interval = codec_.H264()->keyFrameInterval;
		// Codec_settings uses kbits/second; encoder uses bits/second.
		configurations_[i].max_bps = codec_.maxBitrate * 1000;
		configurations_[i].target_bps = codec_.maxBitrate * 1000 / 2;
#else
		configurations_[i].max_frame_rate = 60.0f;
		configurations_[i].frame_dropping_on = false;
		configurations_[i].key_frame_interval = codec_.H264()->keyFrameInterval;
		// Codec_settings uses kbits/second; encoder uses bits/second.
		configurations_[i].max_bps = DJI_MAX_BPS * 1000;
		configurations_[i].target_bps = DJI_MAX_BPS * 1000;
#endif
	
#ifdef  FAKE_NV_ENCODER_USING					
		nv_encoder->SetOption(xop::VE_OPT_WIDTH, configurations_[i].width);
		nv_encoder->SetOption(xop::VE_OPT_HEIGHT, configurations_[i].height);
		nv_encoder->SetOption(xop::VE_OPT_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
		nv_encoder->SetOption(xop::VE_OPT_GOP, configurations_[i].key_frame_interval);
		nv_encoder->SetOption(xop::VE_OPT_CODEC, xop::VE_OPT_CODEC_H264);
		nv_encoder->SetOption(xop::VE_OPT_BITRATE_KBPS, configurations_[i].target_bps / 1000);
		nv_encoder->SetOption(xop::VE_OPT_TEXTURE_FORMAT, xop::VE_OPT_FORMAT_B8G8R8A8);
		if (!nv_encoder->Init()) {
			Release();
			ReportError();
			return WEBRTC_VIDEO_CODEC_ERROR;
		}
#endif		
		encoder_cfg_update(VIDEO_CODEC_H264, configurations_[i].width, configurations_[i].height, static_cast<int>(configurations_[i].max_frame_rate), \
					configurations_[i].key_frame_interval, configurations_[i].target_bps);// update av module
		
		image_buffer_.reset(new uint8_t[configurations_[i].width * configurations_[i].height * 10]);

		// TODO(pbos): Base init params on these values before submitting.
		video_format_ = EVideoFormatType::videoFormatI420;

		// Initialize encoded image. Default buffer size: size of unencoded data.
		const size_t new_capacity = CalcBufferSize(VideoType::kI420, 
			codec_.simulcastStream[idx].width, codec_.simulcastStream[idx].height);
		encoded_images_[i].SetEncodedData(EncodedImageBuffer::Create(new_capacity));
		encoded_images_[i]._completeFrame = true;
		encoded_images_[i]._encodedWidth = codec_.simulcastStream[idx].width;
		encoded_images_[i]._encodedHeight = codec_.simulcastStream[idx].height;
		encoded_images_[i].set_size(0);
	}

	SimulcastRateAllocator init_allocator(codec_);
#ifndef MESH_DJI
	VideoBitrateAllocation allocation = init_allocator.GetAllocation(
		codec_.maxBitrate * 1000 / 2, codec_.maxFramerate);
	SetRates(RateControlParameters(allocation, codec_.maxFramerate));
#else
	VideoBitrateAllocation allocation = init_allocator.GetAllocation(
		DJI_MAX_BPS/3*2, DJI_MAX_BPS);
	SetRates(RateControlParameters(allocation, DJI_MAX_BPS));	
#endif
	while( !encoded_init_ok() )
	{
		usleep(5*1000); // 5ms 
	}
	
	//encoded_data_clean();
	
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "encoder InitEncode ok \n");
	
	return WEBRTC_VIDEO_CODEC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t AvEncoder::Release() 
{
	while (!nv_encoders_.empty()) 
	{		
//#ifdef  FAKE_NV_ENCODER_USING				
		xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_.back());
		if (nv_encoder) {
			nv_encoder->Destroy();
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "encoder release nv_encoder ");
			delete nv_encoder;
		}
//#endif		
		nv_encoders_.pop_back();
	}

	video_recv_stop(2);

	configurations_.clear();
	encoded_images_.clear();

	if(encoded_data_buf_)
	{
		free(encoded_data_buf_);
		encoded_data_buf_ = NULL;
	}
	
	return WEBRTC_VIDEO_CODEC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t AvEncoder::RegisterEncodeCompleteCallback(EncodedImageCallback* callback) 
{
	encoded_image_callback_ = callback;
	return WEBRTC_VIDEO_CODEC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void AvEncoder::SetRates(const RateControlParameters& parameters)
{
	LayerConfig *cfg;
	//int w, h;
	
	if (parameters.bitrate.get_sum_bps() == 0) 
	{
		printf("++++++++++++++++++++ SetRates stop all! ++++++++++++++++++++\n");
		// Encoder paused, turn off all encoding.
		for (size_t i = 0; i < configurations_.size(); ++i)
			configurations_[i].SetStreamState(false);
		return;
	}

	// At this point, bitrate allocation should already match codec settings.
	if (codec_.maxBitrate > 0)
		RTC_DCHECK_LE(parameters.bitrate.get_sum_kbps(), codec_.maxBitrate);
	RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.minBitrate);
	if (codec_.numberOfSimulcastStreams > 0)
		RTC_DCHECK_GE(parameters.bitrate.get_sum_kbps(), codec_.simulcastStream[0].minBitrate);

	codec_.maxFramerate = static_cast<uint32_t>(parameters.framerate_fps);

	size_t stream_idx = nv_encoders_.size() - 1;
	for (size_t i = 0; i < nv_encoders_.size(); ++i, --stream_idx) 
	{
		cfg = &configurations_[i];
		cfg->target_bps     = parameters.bitrate.GetSpatialLayerSum(stream_idx);
		cfg->max_frame_rate = static_cast<float>(parameters.framerate_fps);

		if (cfg->target_bps) 
		{
			cfg->SetStreamState(true);

			if (nv_encoders_[i]) 
			{
#ifdef  FAKE_NV_ENCODER_USING								
				xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_[i]);
				nv_encoder->SetEvent(xop::VE_EVENT_RESET_BITRATE_KBPS, configurations_[i].target_bps/1000);
				nv_encoder->SetEvent(xop::VE_EVENT_RESET_FRAME_RATE, static_cast<int>(configurations_[i].max_frame_rate));
#else				
				//encoder_frame_rate_set(static_cast<int>(configurations_[i].max_frame_rate));
				//encoder_bps_set(configurations_[i].target_bps);
				// update av module cfg
				encoder_cfg_update(VIDEO_CODEC_H264, cfg->width, cfg->height, static_cast<int>(cfg->max_frame_rate), cfg->key_frame_interval, cfg->target_bps);
#endif				
			}
			else 
			{
				printf("++++++++++++++++++++ SetRates stop %ld! ++++++++++++++++++++\n", i);		
				configurations_[i].SetStreamState(false);
			}
		} 
	}
	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t AvEncoder::Encode(const VideoFrame& input_frame,
						  const std::vector<VideoFrameType>* frame_types)
{
	//printf("encode start...... \n");
    static int encode_call_count = 0;
    static uint64_t last_print_time = 0;
    static uint64_t last_encode_time = 0;
    uint64_t now = webrtc::TimeMicros() / 1000; // ms

    encode_call_count++;
    if (last_print_time == 0) {
        last_print_time = now;
        last_encode_time = now;
    }
    if (now - last_print_time >= 5000) { // 每5秒打印一次
        double avg_per_sec = encode_call_count * 1000.0 / (now - last_encode_time);
        MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "AvEncoder::Encode called %.2f times per second in last 5 seconds", avg_per_sec);
        encode_call_count = 0;
        last_print_time = now;
        last_encode_time = now;
    }
				
	if (nv_encoders_.empty()) 
	{
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}
	
	if (!encoded_image_callback_) 
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "++++++++ InitEncode() has been called, but a callback function ++++++++++++ \n");	
		ReportError();
		return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
	}

	webrtc::scoped_refptr<const I420BufferInterface> frame_buffer = input_frame.video_frame_buffer()->ToI420();

	bool send_key_frame = false;
	for (size_t i = 0; i < configurations_.size(); ++i) {
		if (configurations_[i].key_frame_request && configurations_[i].sending) {
			send_key_frame = true;
			break;
		}
	}
	if (!send_key_frame && frame_types) {
		for (size_t i = 0; i < frame_types->size() && i < configurations_.size(); ++i) {
			if ((*frame_types)[i] == VideoFrameType::kVideoFrameKey && configurations_[i].sending) {
				send_key_frame = true;
				break;
			}
		}
	}
	
	RTC_DCHECK_EQ(configurations_[0].width, frame_buffer->width());
	RTC_DCHECK_EQ(configurations_[0].height, frame_buffer->height());

	int data_size;
	// Encode image for each layer.
	for (size_t i = 0; i < nv_encoders_.size(); ++i) {
		if (!configurations_[i].sending) {
			continue;
		}

		if (frame_types != nullptr) {
			// Skip frame?
			if ((*frame_types)[i] == VideoFrameType::kEmptyFrame) {
				continue;
			}
		}

		if (send_key_frame) {			
			if (!nv_encoders_.empty() && nv_encoders_[i]) 
			{
#ifdef  FAKE_NV_ENCODER_USING				
				xop::NvidiaD3D11Encoder* nv_encoder = reinterpret_cast<xop::NvidiaD3D11Encoder*>(nv_encoders_[i]);
				nv_encoder->SetEvent(xop::VE_EVENT_FORCE_IDR, 1);
#else				
				//encoder_force_idr(); // IDR frame created by av module, not need to send event periodly
#endif			
			}
			configurations_[i].key_frame_request = false;
		}

		// EncodeFrame output.
		SFrameBSInfo info;
		memset(&info, 0, sizeof(SFrameBSInfo));
				
		EncodeFrame((int)i, input_frame, encoded_data_buf_, &data_size);
		if (data_size == 0) 
		{	
			return WEBRTC_VIDEO_CODEC_OK;
		}
		else {
			if ((encoded_data_buf_[4] & 0x1f) == 0x07 || (encoded_data_buf_[4] & 0x1f) == 0x08 || (encoded_data_buf_[4] & 0x1f) == 0x05) {
				info.eFrameType = videoFrameTypeIDR; 
			}
			else if ((encoded_data_buf_[4] & 0x1f) == 0x01) {
				info.eFrameType = videoFrameTypeP;
			}
			else {
				printf("encode other frame 0x%02x\n", (encoded_data_buf_[4] & 0x1f));
				info.eFrameType = videoFrameTypeP;
			}
		}

		encoded_images_[i]._encodedWidth    = configurations_[i].width;
		encoded_images_[i]._encodedHeight   = configurations_[i].height;
		encoded_images_[i].SetTimestamp(input_frame.timestamp());
		encoded_images_[i].ntp_time_ms_     = input_frame.ntp_time_ms();
		encoded_images_[i].capture_time_ms_ = input_frame.render_time_ms();
		encoded_images_[i].rotation_        = input_frame.rotation();
		encoded_images_[i].SetColorSpace(input_frame.color_space());
		encoded_images_[i].content_type_ = (codec_.mode == VideoCodecMode::kScreensharing)
											? VideoContentType::SCREENSHARE
											: VideoContentType::UNSPECIFIED;
		encoded_images_[i].timing_.flags = VideoSendTiming::kInvalid;
		encoded_images_[i]._frameType = ConvertToVideoFrameType(info.eFrameType);
		encoded_images_[i].SetSpatialIndex(configurations_[i].simulcast_idx);

		// Split encoded image up into fragments. This also updates
		// |encoded_image_|.
		RTPFragmentationHeader frag_header;

		RtpFragmentize(&encoded_images_[i], *frame_buffer, encoded_data_buf_, data_size, &frag_header);

		// Encoder can skip frames to save bandwidth in which case
		// |encoded_images_[i]._length| == 0.
		if (encoded_images_[i].size() > 0) 
		{
			// Parse QP.
			h264_bitstream_parser_.ParseBitstream(encoded_images_[i].data(), encoded_images_[i].size());
			h264_bitstream_parser_.GetLastSliceQp(&encoded_images_[i].qp_);

			// Deliver encoded image.
			CodecSpecificInfo codec_specific;
			codec_specific.codecType = kVideoCodecH264;
			codec_specific.codecSpecific.H264.packetization_mode = packetization_mode_;
			codec_specific.codecSpecific.H264.temporal_idx    = kNoTemporalIdx;
			codec_specific.codecSpecific.H264.idr_frame       = (info.eFrameType == videoFrameTypeIDR);
			codec_specific.codecSpecific.H264.base_layer_sync = false;
			encoded_image_callback_->OnEncodedImage(encoded_images_[i], &codec_specific, &frag_header);
		}
	}

	return WEBRTC_VIDEO_CODEC_OK;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void AvEncoder::ReportInit() 
{
	if (has_reported_init_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.AvEncoder.Event",	kH264EncoderEventInit, kH264EncoderEventMax);
	has_reported_init_ = true;
}

void AvEncoder::ReportError() 
{
	if (has_reported_error_)
		return;
	RTC_HISTOGRAM_ENUMERATION("WebRTC.Video.AvEncoder.Event",	kH264EncoderEventError, kH264EncoderEventMax);
	has_reported_error_ = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
VideoEncoder::EncoderInfo AvEncoder::GetEncoderInfo() const 
{
	EncoderInfo info;
	info.supports_native_handle  = false;
	info.implementation_name     = "AvEncoder";
	info.scaling_settings        = VideoEncoder::ScalingSettings(kLowH264QpThreshold, kHighH264QpThreshold);
	info.is_hardware_accelerated = true;
	info.has_internal_source     = false;
	return info;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
void AvEncoder::LayerConfig::SetStreamState(bool send_stream) 
{
	if (send_stream && !sending) {
		// Need a key frame if we have not sent this stream before.
		key_frame_request = true;
	}
	sending = send_stream;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// ENCODE_MEMORY_OPT
bool AvEncoder::EncodeFrame(int index, const VideoFrame& input_frame, char *data, int *size) 
{	
	int ret;
	static int  encoded_num = 0;	
	
	*size = 0;	
	ret = encoded_data_get(data, size, &encoded_num);
	if(ret <= 0 )
	{
		return false;
	}
	
	encoded_num++;
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
webrtc::VideoCodec AvEncoder::GetCodecSettings() 
{
//  webrtc::MutexLock lock(&mutex_);
  return codec_settings_;
}

}  // namespace webrtc
