/*
 */

#include "fake_audio_capture_module.h"

#include <string.h>

#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"
#include "pcm.h"
#include "rtp_session.h"
#include "ipc_msg.h"
///////////////////////////////////////////////////////////////////////////////////////////////
namespace FAKE_AUDIO
{

	// Audio sample value that is high enough that it doesn't occur naturally when
	// frames are being faked. E.g. NetEq will not generate this large sample value
	// unless it has received an audio frame containing a sample of this value.
	// Even simpler buffers would likely just contain audio sample values of 0.
	static const int kHighSampleValue = 10000;

	static const int kTimePerFrameMs       = 10; // always 10ms frame
	static const uint8_t kNumberOfChannels = 1; // always one-channel
	static const int kTotalDelayMs         = 0;
	static const int kClockDriftMs         = 0;
	static const uint32_t kMaxVolume       = 14392; //

	static int kSamplesPerSecond = 44100; // 
	static int kSamplesPerFrame  = 441;

	static bool audio_file_using = true;	
#ifdef LINUX_X86 	
	static FILE *fp_in           = NULL;
	const char  *pcm_in_file     = "test.pcm"; 
	const char  *pcm_out_file    = "out.pcm";	
	static FILE *fp_out          = NULL;
#endif

	//////////////////////////////////////////
	static char peer_ip_addr[16]; 
	// default params , do not modify .
	static char local_ip_addr[32]; 
	static int peer_port  = DEFAULT_RTP_AV_ADAPTER_PORT+1;
	static int local_port = DEFAULT_RTP_AV_ADAPTER_PORT;
	static int data_type  = AUDIO_TYPE_PCMA_DATA; 

	enum {
		MSG_START_PROCESS,
		MSG_RUN_PROCESS,
	};

	void mixer_chann_open()
	{
		char buf[64];

		sprintf(buf, "%s%s", IPC_MSG_CFG_CHANN, MSG_ADAPTER_ON);
		ipc_msg_send(MIXER_MODULE_NAME, buf, strlen(buf)+1);
	}
	
	void mixer_chann_close()
	{
		char buf[64];

		sprintf(buf, "%s%s", IPC_MSG_CFG_CHANN, MSG_ADAPTER_OFF);
		ipc_msg_send(MIXER_MODULE_NAME, buf, strlen(buf)+1);
	}	
	
	//////////////////////////////////////////////////////////////////////////////
	int audio_frame_energy(const char *src_audio_data, unsigned int  data_len)
	{
		int  tmp, energy, unit;
		long sum;
		unsigned int i;
	
		sum  = 0;
		unit = 512 * 512; //unit = 1024 * 1024;
		for (i = 0; i < data_len/2; i++)
		{
			tmp    = (int)*(short *)(src_audio_data + i*2);
			tmp    = tmp * tmp;
			energy = tmp/unit;
			sum   += energy;
		}
		sum = sum*2/data_len; // average energy, data is 16bit

		return sum;
	}
	
	FakeAudioCaptureModule::FakeAudioCaptureModule()
		: audio_callback_(nullptr),
		recording_(false),
		playing_(false),
		play_is_initialized_(false),
		rec_is_initialized_(false),
		current_mic_level_(kMaxVolume),
		started_(false),
		next_frame_time_(0),
		frames_received_(0)
	{
		av_rtp_session_id  = -1;
		strcpy(peer_ip_addr, "127.0.0.1");
		strcpy(local_ip_addr, "127.0.0.1");
		ResetRecBuffer();
		memset(send_buffer_, 0, sizeof(send_buffer_));
	}

	FakeAudioCaptureModule::~FakeAudioCaptureModule() 
	{
		if (process_thread_) {
			process_thread_->Stop();
		}
		mixer_chann_close();
		rtp_session_thread_stop(av_rtp_session_id);
	}

	webrtc::scoped_refptr<FakeAudioCaptureModule> FakeAudioCaptureModule::Create(bool file_io, int samples) 
	{
		// global params
		kSamplesPerSecond = samples; // 
		kSamplesPerFrame  = samples/100; // always use 10ms frame
		audio_file_using  = file_io;
	
		webrtc::scoped_refptr<FakeAudioCaptureModule> capture_module(
			new webrtc::RefCountedObject<FakeAudioCaptureModule>());
		if (!capture_module->Initialize()) 
		{
			return nullptr;
		}

		// create audio I/O thread
		if(!audio_file_using)
		{
			mixer_chann_open();
			capture_module->av_rtp_session_id = rtp_session_thread_start("audio_adapter", (const char *)peer_ip_addr, peer_port, local_ip_addr, local_port, data_type);			
		} else {
			MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "audio use file as I/O , not send/recv data to/from audio device!!!");
		}
		
		return capture_module;
	}

	void  FakeAudioCaptureModule::AudioSamplesChange(int samples)
	{
		MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "audio samples changed from %d to %d", kSamplesPerSecond, samples);
		kSamplesPerSecond = samples; // 
		kSamplesPerFrame  = samples/100; //		
	}
	
	int FakeAudioCaptureModule::frames_received() const 
	{
		webrtc::CritScope cs(&crit_);
		return frames_received_;
	}

	int32_t FakeAudioCaptureModule::ActiveAudioLayer(AudioLayer* /*audio_layer*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::RegisterAudioCallback(webrtc::AudioTransport* audio_callback) 
	{
		webrtc::CritScope cs(&crit_callback_);
		audio_callback_ = audio_callback;
		return 0;
	}

	int32_t FakeAudioCaptureModule::Init() 
	{
		// Initialize is called by the factory method. Safe to ignore this Init call.
		// Do nothing
		return 0;
	}

	int32_t FakeAudioCaptureModule::Terminate() 
	{
		// Clean up in the destructor. No action here, just success.
		return 0;
	}

	bool FakeAudioCaptureModule::Initialized() const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int16_t FakeAudioCaptureModule::PlayoutDevices() {
		RTC_NOTREACHED();
		return 0;
	}

	int16_t FakeAudioCaptureModule::RecordingDevices() {
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::PlayoutDeviceName(
		uint16_t /*index*/,
		char /*name*/[webrtc::kAdmMaxDeviceNameSize],
		char /*guid*/[webrtc::kAdmMaxGuidSize]) 
	{
		RTC_NOTREACHED();
		MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "PlayoutDeviceName \n");
		return 0;
	}

	int32_t FakeAudioCaptureModule::RecordingDeviceName(
		uint16_t /*index*/,
		char /*name*/[webrtc::kAdmMaxDeviceNameSize],
		char /*guid*/[webrtc::kAdmMaxGuidSize]) 
	{
		RTC_NOTREACHED();
		MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "RecordingDeviceName \n");
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetPlayoutDevice(uint16_t /*index*/) 
	{
		// No playout device, just playing from file. Return success.
		MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "SetPlayoutDevice \n");
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetPlayoutDevice(WindowsDeviceType /*device*/) 
	{
		if (play_is_initialized_) {
			return -1;
		}
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetRecordingDevice(uint16_t /*index*/) 
	{
		// No recording device, just dropping audio. Return success.
		MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "SetRecordingDevice  \n");
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetRecordingDevice(
		WindowsDeviceType /*device*/) 
	{
		if (rec_is_initialized_) 
		{
			return -1;
		}
		return 0;
	}

	int32_t FakeAudioCaptureModule::PlayoutIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::InitPlayout() 
	{
		play_is_initialized_ = true;
		return 0;
	}

	bool FakeAudioCaptureModule::PlayoutIsInitialized() const 
	{
		return play_is_initialized_;
	}

	int32_t FakeAudioCaptureModule::RecordingIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::InitRecording() 
	{
		rec_is_initialized_ = true;
		return 0;
	}

	bool FakeAudioCaptureModule::RecordingIsInitialized() const 
	{
		return rec_is_initialized_;
	}

	int32_t FakeAudioCaptureModule::StartPlayout() 
	{
		if (!play_is_initialized_) {
			return -1;
		}
		{
			webrtc::CritScope cs(&crit_);
			playing_ = true;
		}
		bool start = true;
		UpdateProcessing(start);
		return 0;
	}

	int32_t FakeAudioCaptureModule::StopPlayout() 
	{
		bool start = false;
		{
			webrtc::CritScope cs(&crit_);
			playing_ = false;
			start = ShouldStartProcessing();
		}
		UpdateProcessing(start);
		return 0;
	}

	bool FakeAudioCaptureModule::Playing() const 
	{
		webrtc::CritScope cs(&crit_);
		return playing_;
	}

	int32_t FakeAudioCaptureModule::StartRecording() 
	{
		if (!rec_is_initialized_) {
			return -1;
		}
		{
			webrtc::CritScope cs(&crit_);
			recording_ = true;
		}
		bool start = true;
		UpdateProcessing(start);
		return 0;
	}

	int32_t FakeAudioCaptureModule::StopRecording() 
	{
		bool start = false;
		{
			webrtc::CritScope cs(&crit_);
			recording_ = false;
			start = ShouldStartProcessing();
		}
		UpdateProcessing(start);
		return 0;
	}

	bool FakeAudioCaptureModule::Recording() const 
	{
		webrtc::CritScope cs(&crit_);
		return recording_;
	}

	int32_t FakeAudioCaptureModule::InitSpeaker() 
	{
		// No speaker, just playing from file. Return success.
		return 0;
	}

	bool FakeAudioCaptureModule::SpeakerIsInitialized() const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::InitMicrophone() 
	{
		// No microphone, just playing from file. Return success.
		return 0;
	}

	bool FakeAudioCaptureModule::MicrophoneIsInitialized() const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SpeakerVolumeIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetSpeakerVolume(uint32_t /*volume*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SpeakerVolume(uint32_t* /*volume*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::MaxSpeakerVolume(uint32_t* /*max_volume*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::MinSpeakerVolume(uint32_t* /*min_volume*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::MicrophoneVolumeIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetMicrophoneVolume(uint32_t volume) 
	{
		webrtc::CritScope cs(&crit_);
		current_mic_level_ = volume;
		return 0;
	}

	int32_t FakeAudioCaptureModule::MicrophoneVolume(uint32_t* volume) const 
	{
		webrtc::CritScope cs(&crit_);
		*volume = current_mic_level_;
		return 0;
	}

	int32_t FakeAudioCaptureModule::MaxMicrophoneVolume(uint32_t* max_volume) const 
	{
		*max_volume = kMaxVolume;
		return 0;
	}

	int32_t FakeAudioCaptureModule::MinMicrophoneVolume(uint32_t* /*min_volume*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SpeakerMuteIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetSpeakerMute(bool /*enable*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SpeakerMute(bool* /*enabled*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::MicrophoneMuteIsAvailable(bool* /*available*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetMicrophoneMute(bool /*enable*/) 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::MicrophoneMute(bool* /*enabled*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::StereoPlayoutIsAvailable(bool* available) const 
	{
		// No recording device, just dropping audio. Stereo can be dropped just
		// as easily as mono.
		*available = true;
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetStereoPlayout(bool /*enable*/) 
	{
		// No recording device, just dropping audio. Stereo can be dropped just
		// as easily as mono.
		return 0;
	}

	int32_t FakeAudioCaptureModule::StereoPlayout(bool* /*enabled*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::StereoRecordingIsAvailable(	bool* available) const 
	{
		// Keep thing simple. No stereo recording.
		*available = false;
		return 0;
	}

	int32_t FakeAudioCaptureModule::SetStereoRecording(bool enable) 
	{
		if (!enable) {
			return 0;
		}
		return -1;
	}

	int32_t FakeAudioCaptureModule::StereoRecording(bool* /*enabled*/) const 
	{
		RTC_NOTREACHED();
		return 0;
	}

	int32_t FakeAudioCaptureModule::PlayoutDelay(uint16_t* delay_ms) const 
	{
		// No delay since audio frames are dropped.
		*delay_ms = 0;
		return 0;
	}

	void FakeAudioCaptureModule::OnMessage(webrtc::Message* msg) 
	{
		switch (msg->message_id) {
		case MSG_START_PROCESS:
			StartProcessP();
			break;
		case MSG_RUN_PROCESS:
			ProcessFrameP();
			break;
		default:
			// All existing messages should be caught. Getting here should never
			// happen.
			RTC_NOTREACHED();
		}
	}

	bool FakeAudioCaptureModule::Initialize() 
	{
		// Set the send buffer samples high enough that it would not occur on the
		// remote side unless a packet containing a sample of that magnitude has been
		// sent to it. Note that the audio processing pipeline will likely distort the
		// original signal.

		//SetSendBuffer(kHighSampleValue); // we use pcm data from file. not manually set buffer content

		return true;
	}

	void FakeAudioCaptureModule::SetSendBuffer(int value) 
	{
		Sample* buffer_ptr = reinterpret_cast<Sample*>(send_buffer_);
		const size_t buffer_size_in_samples = sizeof(send_buffer_) / kNumberBytesPerSample;
		for (size_t i = 0; i < buffer_size_in_samples; ++i) {
			buffer_ptr[i] = value;
		}
	}

	void FakeAudioCaptureModule::ResetRecBuffer() 
	{
		memset(rec_buffer_, 0, sizeof(rec_buffer_));
	}

	bool FakeAudioCaptureModule::CheckRecBuffer(int value) 
	{
		const Sample* buffer_ptr = reinterpret_cast<const Sample*>(rec_buffer_);
		const size_t buffer_size_in_samples =
			sizeof(rec_buffer_) / kNumberBytesPerSample;
		for (size_t i = 0; i < buffer_size_in_samples; ++i) 
		{
			if (buffer_ptr[i] >= value)
				return true;
		}
		return false;
	}

	bool FakeAudioCaptureModule::ShouldStartProcessing() 
	{
		return recording_ || playing_;
	}

	void FakeAudioCaptureModule::UpdateProcessing(bool start) 
	{
		if (start) {
			if (!process_thread_) {
				process_thread_ = webrtc::Thread::Create();
				process_thread_->Start();
			}
			process_thread_->Post(RTC_FROM_HERE, this, MSG_START_PROCESS);
		}
		else 
		{
			if (process_thread_) {
				process_thread_->Stop();
				process_thread_.reset(nullptr);
			}
			started_ = false;
		}
	}

	void FakeAudioCaptureModule::StartProcessP() 
	{
		RTC_CHECK(process_thread_->IsCurrent());
		if (started_) 
		{
			// Already started.
			return;
		}
		ProcessFrameP();
	}

	void FakeAudioCaptureModule::ProcessFrameP() 
	{
		RTC_CHECK(process_thread_->IsCurrent());
		if (!started_) {
			next_frame_time_ = webrtc::TimeMillis();
			started_ = true;
		}

		{
			webrtc::CritScope cs(&crit_);
			// Receive and send frames every kTimePerFrameMs.
			if (playing_) {
				ReceiveFrameP();
			}
			if (recording_) {
				SendFrameP();
			}
		}

		next_frame_time_ += kTimePerFrameMs;
		const int64_t current_time = webrtc::TimeMillis();
		const int64_t wait_time =
			(next_frame_time_ > current_time) ? next_frame_time_ - current_time : 0;
		process_thread_->PostDelayed(RTC_FROM_HERE, wait_time, this, MSG_RUN_PROCESS);
	}

	void FakeAudioCaptureModule::ReceiveFrameP()
	{
		//static int test_count = 0;
		size_t nSamplesOut = 0;
		//int ret;

		RTC_CHECK(process_thread_->IsCurrent());
		{
			webrtc::CritScope cs(&crit_callback_);
			if (!audio_callback_) {
				return;
			}

			// clear buffer
			ResetRecBuffer();

			// get decoded audio data from transport
			int64_t elapsed_time_ms = 0;
			int64_t ntp_time_ms = 0;
			if (audio_callback_->NeedMorePlayData(
				kSamplesPerFrame, kNumberBytesPerSample, kNumberOfChannels,
				kSamplesPerSecond, rec_buffer_, nSamplesOut, &elapsed_time_ms,
				&ntp_time_ms) != 0) 
			{
				RTC_NOTREACHED();
			}

			//MESH_LOG_PRINT(AUDIO_DEV_MODULE_NAME, LOG_INFO, "nSamplesOut %d == kSamplesPerFrame %d \n", nSamplesOut, kSamplesPerFrame);
			RTC_CHECK(nSamplesOut == kSamplesPerFrame);
		}

		if (0 == nSamplesOut)
		{
			return;
		}

		if (audio_file_using)
		{
#ifdef LINUX_X86 			
			// get audio data from audio callback (transport layer) and play on  virtual device   
			if (fp_out == NULL)
			{
				fp_out = pcm_open_write(pcm_out_file);
			}
			if (fp_out)
			{
				pcm_file_write(fp_out, rec_buffer_, kSamplesPerFrame * kNumberBytesPerSample);
			}
#endif			
		} else {
			// printf("rtp_session_data_enqueue \n");
			if(audio_output_allowed())
			{
				// discard very low energy audio to keep silent.
				int energy = audio_frame_energy(rec_buffer_, kSamplesPerFrame * kNumberBytesPerSample);
				if(energy > 2) {
					rtp_session_data_enqueue(av_rtp_session_id, (char *)rec_buffer_, kSamplesPerFrame * kNumberBytesPerSample);
				}
			}
		}			
			

		// The SetBuffer() function ensures that after decoding, the audio buffer
		// should contain samples of similar magnitude (there is likely to be some
		// distortion due to the audio pipeline). If one sample is detected to
		// have the same or greater magnitude somewhere in the frame, an actual frame
		// has been received from the remote side (i.e. faked frames are not being
		// pulled).
		// check audio data is ok
		if (CheckRecBuffer(kHighSampleValue))
		{
			webrtc::CritScope cs(&crit_);
			++frames_received_;
		}
	}

	void FakeAudioCaptureModule::SendFrameP()
	{
		int ret = 0;

		RTC_CHECK(process_thread_->IsCurrent());
		webrtc::CritScope cs(&crit_callback_);

		if (!audio_callback_) {
			return;
		}

		if (audio_file_using)
		{
#ifdef LINUX_X86 
			if (fp_in == NULL)
			{
				fp_in = pcm_open_read(pcm_in_file);
			}
			if (fp_in)
			{
				ret = pcm_file_read(fp_in, kSamplesPerFrame, kNumberBytesPerSample, send_buffer_, kSamplesPerFrame * kNumberBytesPerSample, 1);
			}
#endif
		} else {
			// get audio pcm data from I/O thread
			int size;
			ret = rtp_session_data_dequeue(av_rtp_session_id, (char *)send_buffer_, &size, kSamplesPerFrame * kNumberBytesPerSample);
			
		}
		// get audio data from virtual device and return to audio callback (transport layer)
		if (ret == 0)
		{
			return;
		}
		//printf("rtp_session_data_dequeue \n");
		
		bool key_pressed = false;
		uint32_t current_mic_level = 0;
		MicrophoneVolume(&current_mic_level);

		if (audio_callback_->RecordedDataIsAvailable(
			send_buffer_, kSamplesPerFrame, kNumberBytesPerSample,
			kNumberOfChannels, kSamplesPerSecond, kTotalDelayMs, kClockDriftMs,
			current_mic_level, key_pressed, current_mic_level) != 0) 
		{
			RTC_NOTREACHED();
		}

		SetMicrophoneVolume(current_mic_level);
	}

}