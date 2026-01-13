#include <rtc_base/atomic_ops.h>

#include "api/scoped_refptr.h"
#include "media/base/video_common.h"
#include "rtc_base/logging.h"

#include <libyuv.h>

#include "bmp.h"
#include "fake_video_capture.h"
#include "mesh_rtc_inc.h"

#include "cjson.h"
#include "cfg.h"
#include "av_api.h"
#include "../av_cfg.h"
#include "video_data_recv.h"
#include "utils.h"

#define TEST_IMAGE_COUNT 9000

#define IMAGE_TRIGGER_INTERVAL 26 // ms
static int encoder_continuous_discard_frame = 0; // discard frame count, default is 0
////////////////////////////////////////////////////////////////////////////////////////////////////
namespace FAKE_VIDEO
{

////////////////////////////////////////////////////////////////////////////////////////////////////	
	webrtc::scoped_refptr<FakeVideoCapturer> FakeVideoCapturer::Create(int fps)
	{
		webrtc::scoped_refptr<FakeVideoCapturer> implementation(new webrtc::RefCountedObject<FakeVideoCapturer>(fps));
		return implementation;
	}

	void FakeVideoCapturer::AddRef() const
	{
		webrtc::AtomicOps::Increment(&ref_count_);
	}

	webrtc::RefCountReleaseStatus FakeVideoCapturer::Release() const
	{
		const int count = webrtc::AtomicOps::Decrement(&ref_count_);
		if (count == 0) {
			return webrtc::RefCountReleaseStatus::kDroppedLastRef;
		}
		return webrtc::RefCountReleaseStatus::kOtherRefsRemained;
	}

	webrtc::MediaSourceInterface::SourceState FakeVideoCapturer::state() const
	{
		return webrtc::MediaSourceInterface::kLive;
		//              return SourceState();
	}

	bool FakeVideoCapturer::remote() const
	{
		return false;
	}

	bool FakeVideoCapturer::is_screencast() const
	{
		return true;
	}

	absl::optional<bool> FakeVideoCapturer::needs_denoising() const
	{
		return false;

	}

	void FakeVideoCapturer::local_image_load(void)
	{
		uint8_t* imageData = (uint8_t*)new uint8_t[8 * 1024 * 1024];
		unsigned int offset;
		unsigned int max_size = 8 * 1024 * 1024;
		unsigned int imageWidth;
		unsigned int imageHeight;
		unsigned int imagePixelFormat;
		char imgFileName[FILE_NAME_MAX_LEN] = { 0 };
		int ret;

#ifdef LINUX_X86
		strcpy(imgFileName, "./no_video.bmp");	// ffmpeg -i no_video-1.bmp -pix_fmt bgr24 no_video.bmp	
#else
		strcpy(imgFileName, "/system/ui-resources/image/no_video.bmp");		
#endif	
		ret = get_from_bmp_file(imgFileName, imageData, max_size, &imageWidth, &imageHeight, &imagePixelFormat, &offset);
		if(ret < 0)
		{
			delete[] imageData;
			return;
		}
		
		int width = imageWidth;
		int height = imageHeight;

		if (!mI420YUVBbuffer.get() || mI420YUVBbuffer->width() * mI420YUVBbuffer->height() < width * height)
		{
			mI420YUVBbuffer = webrtc::I420Buffer::Create(width, height);
		}

		int stride = width;
		uint8_t* yplane = mI420YUVBbuffer->MutableDataY();
		uint8_t* uplane = mI420YUVBbuffer->MutableDataU();
		uint8_t* vplane = mI420YUVBbuffer->MutableDataV();
		libyuv::ConvertToI420(imageData + offset, 0, yplane, stride, uplane,
						(stride + 1) / 2, vplane, (stride + 1) / 2, 0, 0,
						width, height, width, height, libyuv::kRotate0,
						libyuv::FOURCC_24BG);

		//here will copy data , should relase by API level
		webrtc::VideoFrame myYUVFrame = webrtc::VideoFrame(mI420YUVBbuffer, 0, 0, webrtc::kVideoRotation_0);

		mImageIndex++;
		if (mImageIndex > TEST_IMAGE_COUNT)
		{
			mImageIndex = 0;
		}
		if(mImageIndex == 0)
		{
			MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, "load image and send to rtc encoder: h=%d  w=%d \n",  myYUVFrame.height(), myYUVFrame.width());
		}
		this->OnFrame(myYUVFrame); // image frame output into upper by OnFrame interface !!!

		//Release memory
		delete[] imageData;
		imageData = nullptr;
	}

    ////////////////////////////////////////////////////////////////////
	void FakeVideoCapturer::StartCapture(bool local)
	{
		if (mIsStarted)
		{
			return;
		}
		
		MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, "FakeVideoCapturer::StartCapture local video = %d", local);
		local_src  = local;
		mIsStarted = true;
		
		// Start new thread to capture
		mCaptureThread.reset(new std::thread([this]()
			{
				mImageIndex = 0;
				
				if(local_src)
				{
					while (mIsStarted)
					{
						local_image_load();
						std::this_thread::sleep_for(std::chrono::milliseconds(1000 / mFPS));
					}
				} 
				else
				{
					video_recv_start(NULL, 0, NULL, 0);
					while (mIsStarted)
					{
						int ret = encoded_data_trigger();
						if(ret == 0)
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(5)); 
							continue;
						}

						trigger_image_load();
						sleep_ms(IMAGE_TRIGGER_INTERVAL);
					}				
					video_recv_stop(1);
				}
			}));
	}

	void FakeVideoCapturer::StopCapture() 
	{
		mIsStarted = false;

		if (mCaptureThread && mCaptureThread->joinable())
		{
			mCaptureThread->join(); // wait for load image thread finish
		}
	}

	void FakeVideoCapturer::trigger_image_load(void)
	{
		// mesh terminal only has a 640*480 screen
		int width;
		int height;
		int w, h;

		static int call_count = 0;
		static uint64_t last_print_time = 0;
		static uint64_t last_call_time = 0;
		uint64_t now = webrtc::TimeMillis();
		call_count++;
		if (last_print_time == 0) {
			last_print_time = now;
			last_call_time = now;
		}
		if (now - last_print_time >= 5000) { // 每5秒打印一次
		    double avg_per_sec = call_count * 1000.0 / (now - last_call_time);
			MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, "[FakeVideoCapturer]trigger image load called %.2f times per second in last 5 seconds", avg_per_sec);
			call_count = 0;
			last_print_time = now;
			last_call_time = now;
		}
		
		encoder_wh_get(&width, &height);
		// limit to the max resolution of scrren, such as 640*480 pixel on Z800A/B
		av_wh_max_get(&w, &h);
		if((width > w) || (height > h))
		{
			width  = w;
			height = h;
		}
		
		av_video_res_get(&width, &height);
		if (!mI420YUVBbuffer.get() || mI420YUVBbuffer->width() * mI420YUVBbuffer->height() < width * height)
		{
			mI420YUVBbuffer = webrtc::I420Buffer::Create(width, height);
		}
		
		int out_width, out_height, crop_width, crop_height;
		int crop_x, crop_y;
		static int frame_discard_num = 0;
		// judege if allowed to input more frame
		bool allowed = this->AdaptFrame(width,	height,	1000 * webrtc::TimeMillis()/* us */,
							&out_width, &out_height, 
							&crop_width, &crop_height,
							&crop_x, &crop_y);
		if(!allowed)
		{
			frame_discard_num++;
			encoder_continuous_discard_frame++;
			if(frame_discard_num % 50 == 0)
			{
				MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, " stream encoder not allow to input more frame. frame_discard_num=%d ", frame_discard_num);
				if(encoder_continuous_discard_frame > 150)
				{
					MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, " stream encoder error,restart rtc_client");
					exit(0);
				}
			}
			return;
		}
		encoder_continuous_discard_frame = 0; // reset discard frame count
		webrtc::VideoFrame	triggerFrame = webrtc::VideoFrame::Builder()
				.set_video_frame_buffer(mI420YUVBbuffer)
				.set_timestamp_rtp(0)
				.set_timestamp_ms(webrtc::TimeMillis())
				.set_rotation(webrtc::kVideoRotation_0)
				.build();
		
		
		triggerFrame.set_ntp_time_ms(webrtc::TimeMillis());

		mImageIndex++;
		if (mImageIndex % TEST_IMAGE_COUNT == 0)
		{
			MESH_LOG_PRINT(VIDEO_DEV_MODULE_NAME, LOG_INFO, "load data and send to rtc encoder: h=%d  w=%d mImageIndex=%d capture time=%ld\n",  triggerFrame.height(), triggerFrame.width(), mImageIndex, webrtc::TimeMillis());
		}
		
		// image frame output into upper by OnFrame interface ! In fact we not use this data in encoder !
		this->OnFrame(triggerFrame); 
	}	
////////////////////////////////////////////////////////////////////////////////////////////////////
}  // FAKE_VIDEO