#include "external_video_encoder_factory.h"
#include "absl/memory/memory.h"
#include "media/engine/internal_decoder_factory.h"
#include "rtc_base/logging.h"
#include "absl/strings/match.h"
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"
#include "modules/video_coding/codecs/vp9/include/vp9.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "api/video_codecs/sdp_video_format.h"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"
#include "av_encoder.h"

namespace webrtc {

class ExternalEncoderFactory : public webrtc::VideoEncoderFactory {
public:
	std::vector<webrtc::SdpVideoFormat> GetSupportedFormats()
		const override {
		std::vector<webrtc::SdpVideoFormat> video_formats;
		for (const webrtc::SdpVideoFormat& h264_format : webrtc::SupportedH264Codecs())
			video_formats.push_back(h264_format);
		printf("[ExternalEncoderFactory]GetSupportedFormats ...\n");	
		return video_formats;
	}

	VideoEncoderFactory::CodecInfo QueryVideoEncoder(
		const webrtc::SdpVideoFormat& format) const override {
		CodecInfo codec_info = { false, false };
		codec_info.is_hardware_accelerated = true; // 
		codec_info.has_internal_source     = false;
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory]QueryVideoEncoder ...\n");
		return codec_info;
	}

	// static
	std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder() {
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory]VideoCodec create .............\n");
		std::vector<webrtc::SdpVideoFormat> format = GetSupportedFormats();
		return absl::make_unique<webrtc::AvEncoder>(cricket::VideoCodec(format[0]));
	}

	std::unique_ptr<webrtc::VideoEncoder> CreateVideoEncoder(
		const webrtc::SdpVideoFormat& format) override 
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory]CreateVideoEncoder start .............");
		//if (absl::EqualsIgnoreCase(format.name, cricket::kH264CodecName)) 
		if (0 == strcmp(format.name.c_str(), cricket::kH264CodecName)) 	
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory] H264Encoder::IsSupported() %d .............", webrtc::H264Encoder::IsSupported());
			if (webrtc::H264Encoder::IsSupported()) 
			{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory]VideoCodec create .............");
				return absl::make_unique<webrtc::AvEncoder>(cricket::VideoCodec(format));
			} else {
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ExternalEncoderFactory]codec H264 is not supported ?");
			}
		}

		return nullptr;
	}
};

std::unique_ptr<webrtc::VideoEncoderFactory> CreateBuiltinExternalVideoEncoderFactory() {
	return absl::make_unique<ExternalEncoderFactory>();
}

}


