/*
*/
#ifdef MAIN_WND_USING	
#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <glib-object.h>
#include <glib.h>
#include <gobject/gclosure.h>
#include <gtk/gtk.h>
#endif // MAIN_WND_USING	

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <map>
#include <utility>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

#ifdef MAIN_WND_USING
#include "../main_wnd.h"
#else
#include "../conn_ctrl.h"
#endif

#include "mesh_rtc_common.h"
#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"
#include "ipc_msg.h"

#include "bmp.h"
#include "fake_video_render.h"

#define CONVERT_PHOTO  1  // convet up-down/right-left

#ifndef 	UI_RENDER_VIDEO_INNER_CLASS
///////////////////////////////////////////////////////////////////////////////////
extern int screen_width_max  ;
extern int screen_height_max ;

int hp_screen_width_max  = 640;
int hp_screen_height_max = 480;

int  show_first_photo = 0;
///////////////////////////////////////////////////////////////////////////////////
#ifdef MAIN_WND_USING
VideoRenderer::VideoRenderer(
    GtkMainWnd* main_wnd,
    webrtc::VideoTrackInterface* track_to_render)
    : width_(0),
      height_(0),
      main_wnd_(main_wnd),
      rendered_track_(track_to_render) 
{
  rendered_track_->AddOrUpdateSink(this, webrtc::VideoSinkWants());
}
#else
VideoRenderer::VideoRenderer(
    ConnCtrl* main_wnd,
    webrtc::VideoTrackInterface* track_to_render)
    : width_(0),
      height_(0),
      main_wnd_(main_wnd),
      rendered_track_(track_to_render) 
{
  rendered_track_->AddOrUpdateSink(this, webrtc::VideoSinkWants());
  MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "add track to render");
  show_first_photo = 1;
}	
#endif // MAIN_WND_USING

///////////////////////////////////////////////////////////////////////////////////
VideoRenderer::~VideoRenderer() 
{
  rendered_track_->RemoveSink(this);
}

///////////////////////////////////////////////////////////////////////////////////
void VideoRenderer::SetSize(int width, int height) 
{
#ifdef MAIN_WND_USING	
  gdk_threads_enter();
#endif

  if (width_ == width && height_ == height) {
    return;
  }

  width_ = width;
  height_ = height;
  image_.reset(new uint8_t[width * height * 4]);
  
    MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "set image size w=%d h=%d", width, height);
	
#ifdef MAIN_WND_USING	 
  gdk_threads_leave();
#endif  
}

///////////////////////////////////////////////////////////////////////////////////
bool RGB24RotateHorizontal(unsigned char* rgb24, unsigned char *rgb_out, int width, int height)
{
	if (rgb24 == NULL || width <= 0 || height <= 0 || rgb_out == NULL) 
	{
		return false;
	}
 
	int bytes_of_line = (width * 24 + 23) / 24 * 3;  
	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			std::memcpy((rgb_out + bytes_of_line * (height - 1 - i) + 3 * (width - 1 - j)), (rgb24 + bytes_of_line * (height - 1 - i) + 3 * j), 3);
		}
	}
	
	return true;
}
///////////////////////////////////////////////////////////////////////////////////
struct rgb24_data_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

void RGB24RotateVertical(unsigned char* rgb24_data, int width, int height)
{	
	rgb24_data_t *rgb24Buf = (rgb24_data_t *)rgb24_data;
	std::reverse(rgb24Buf, rgb24Buf + width * height); 
}
///////////////////////////////////////////////////////////////////////////////////
void VideoRenderer::OnFrame(const webrtc::VideoFrame& video_frame) 
{
	static int frame_num   = 0;
	static int discard_num = 0;
	struct stat st;
	const char *file_format2 = "/tmp/remote_frame_%d_%d_%d.bmp";
	char file_name2[128];
	
	if(video_frame.width() * video_frame.height() > (hp_screen_width_max * hp_screen_height_max))
	{
		discard_num++;
		if((discard_num % 20 != 0) && !show_first_photo)
		{
			return; // If frame size > max screen area, discard it !!!		
		}
		show_first_photo = 0;
		// still display photo to notice user 
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "!!!! discard_num=%d width=%d height=%d (screen_width_max=%d * screen_height_max=%d)", \
														discard_num, video_frame.width(), video_frame.height(), screen_width_max, screen_height_max);
	}
	
	sprintf(file_name2, file_format2, video_frame.width(), video_frame.height(), frame_num++ % 30);
	if (stat(file_name2, &st) != -1) 
	{
		return; // file exist? not overwrite it!
	}

#if CONVERT_PHOTO		
	webrtc::scoped_refptr<webrtc::I420BufferInterface> vfb(video_frame.video_frame_buffer()->ToI420());
	vfb = webrtc::I420Buffer::Rotate(*vfb, webrtc::kVideoRotation_180);
#else
	webrtc::scoped_refptr<webrtc::VideoFrameBuffer> vfb = video_frame.video_frame_buffer();
#endif
	
	// convert to bmp and save to file for mesh ui 
	uint8_t *p;
	p = (uint8_t *)malloc(sizeof(uint8_t) * video_frame.width() * video_frame.height() * 4);
	if(!p)
	{
		return;
	}
		
	libyuv::I420ToRGB24(vfb.get()->GetI420()->DataY(), video_frame.width(), vfb.get()->GetI420()->DataU(),
		    video_frame.width() / 2, vfb.get()->GetI420()->DataV(), video_frame.width() / 2, p,
		    video_frame.width() * 3, video_frame.width(), video_frame.height());

#if CONVERT_PHOTO		
	uint8_t *q;
	q = (uint8_t *)malloc(sizeof(uint8_t) * video_frame.width() * video_frame.height() * 4);
	if(q)
	{
		RGB24RotateHorizontal(p, q, video_frame.width(), video_frame.height());
		save_to_bmp_file(file_name2, q, video_frame.width(), video_frame.height(), 24);
		free(q);
	} else {
		free(p); // discard this frame data
		return;
	}
#else		
	save_to_bmp_file(file_name2, p, video_frame.width(), video_frame.height(), 24);			
#endif
		
	if(frame_num % 90 == 0)
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "frame_num=%d discard_num=%d . save_to_bmp_file %s +++ rotation: %d", frame_num, discard_num, file_name2, video_frame.rotation());
	}
			
	// send photo update cmd to mesh ui 
	char update_photo[512];
	memset(update_photo, 0, 512);
	sprintf(update_photo, MESH_PHOTO_MSG_FORMAT, MESH_PHOTO_UPDATE, file_name2);
	ipc_msg_send(MESH_UI_PHOTO_MODULE_NAME, update_photo, strlen(update_photo)+1);

	free(p); 
}


#endif