#ifndef  __MESH_RTC_VIDEO_RENDER_H__
#define  __MESH_RTC_VIDEO_RENDER_H__

#include <stdint.h>
#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"

#ifdef MAIN_WND_USING
#include "../main_wnd.h"
#else
#include "../conn_ctrl.h"
#endif

#include "../peer_connection_mesh.h"

#ifndef 	UI_RENDER_VIDEO_INNER_CLASS

class VideoRenderer : public webrtc::VideoSinkInterface<webrtc::VideoFrame> 
{
public:
#ifdef MAIN_WND_USING 
    VideoRenderer(GtkMainWnd* main_wnd, webrtc::VideoTrackInterface* track_to_render);
#else
	VideoRenderer(ConnCtrl* main_wnd, webrtc::VideoTrackInterface* track_to_render);
#endif	
    virtual ~VideoRenderer();

    // VideoSinkInterface implementation
    void OnFrame(const webrtc::VideoFrame& frame) override;

    const uint8_t* image() const { return image_.get(); }

    int width() const { return width_; }

    int height() const { return height_; }

protected:
    void SetSize(int width, int height);
    std::unique_ptr<uint8_t[]> image_;
    int width_;
    int height_;
	
#ifdef MAIN_WND_USING 	
    GtkMainWnd* main_wnd_;
#else
    ConnCtrl* main_wnd_;
#endif	
    webrtc::scoped_refptr<webrtc::VideoTrackInterface> rendered_track_;
};
  
#endif
  
#endif  