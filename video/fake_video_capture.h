#pragma once
 
#include <string>
#include <thread>
 
#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
 
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/strings/json.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/desktop_frame.h"
#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/cropping_window_capturer.h"
#include "media/base/adapted_video_track_source.h"
#include "api/video/i420_buffer.h"
 
 //////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
namespace FAKE_VIDEO
{
    class FakeVideoCapturer :public webrtc::AdaptedVideoTrackSource
    {
    public:
        explicit FakeVideoCapturer(size_t fps) : ref_count_(0), mFPS(fps), mImageIndex(0) { mIsStarted = false;};
        //
        void StartCapture(bool local);
        void StopCapture();
		//
		static webrtc::scoped_refptr<FakeVideoCapturer> Create(int fps); 
 
        // 通过 AdaptedVideoTrackSource 继承
        virtual webrtc::MediaSourceInterface::SourceState state() const override;
        virtual bool remote() const override;
        virtual bool is_screencast() const override;
        virtual absl::optional<bool> needs_denoising() const override;

        virtual void AddRef() const override;
        virtual webrtc::RefCountReleaseStatus Release() const override;

    private:
        mutable volatile int ref_count_;
        std::atomic_bool mIsStarted;

		void local_image_load(void);
		void trigger_image_load(void);
		
        std::unique_ptr<std::thread> mCaptureThread;
        size_t mFPS;
 
        webrtc::scoped_refptr<webrtc::I420Buffer> mI420YUVBbuffer;
 
        unsigned int mImageIndex;		
		bool local_src;
    };

} //FAKE_VIDEO