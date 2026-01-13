/*
 */

#ifndef __MAIN_CTRL_H_
#define __MAIN_CTRL_H_

#ifndef MAIN_WND_USING

#include <stdint.h>

#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/scoped_refptr.h"
#include "api/video/video_frame.h"
#include "api/video/video_sink_interface.h"
#include "main_wnd_basic.h"
#include "peer_connection_mesh.h"
#include "peer_connection_client.h"

class VideoRenderer;
//////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum enum_pc_mesh_conn_status {
	PC_CONN_MESH_IDLE,
	PC_CONN_MESH_LOGINING,
	PC_CONN_MESH_LOGINED,
	PC_CONN_MESH_CONNECTING,
	PC_CONN_MESH_CONNECTED,
	PC_CONN_MESH_DISCONNECTING,
}pc_mesh_conn_status_e;

typedef enum enum_pc_server_conn_status {
	PC_CONN_SERVER_IDLE = 10,
	PC_CONN_SERVER_LOGINING,
	PC_CONN_SERVER_LOGINED,
	PC_CONN_SERVER_CONNECTED
}pc_server_conn_status_e;


typedef enum enum_app_rtc_cmd {
	MESH_LOGIN = 0,
	MESH_CONN,
	MESH_DISCONN,
	MESH_LOGOUT,
		
	RTC_MEDIA_SDP_SEND,
	RTC_DISCONN,
	RTC_LOGIN,
	RTC_LOGIN_OK,
	RTC_LOGOUT,
	RTC_LOGOUT_MSG,
	RTC_HDMI_CALL,
	
	DJI_STREAM_NAME_SET,
	RTC_DJI_STREAM_STOP,

	MESH_SERVER_SET,
	RTC_SERVER_SET,
	RTC_MEDIA_SET,
	SWITCH_TO_RTC_SERVER,
	SWITCH_TO_MESH_SERVER
}app_rtc_cmd_e;


enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    NEW_TRACK_ADDED,
    TRACK_REMOVED,
	
	WEBRTC_SERVER_MESSAGE_SEND,
	WEBRTC_SERVER_CALL,
	WEBRTC_SERVER_HANGUP,
	WEBRTC_SERVER_LOGINED,
	WEBRTC_SERVER_LOGOUT	
};

const char *cmd_string(int cmd);
const char *mesh_conn_status_string(int status);
const char *server_conn_status_string(int status);
//////////////////////////////////////////////////////////////////////////////////////////////////
// Implements the main contrl flow of the peer connection client.

class ConnCtrl : public MainWindow {
	
public:
	ConnCtrl(const char* server, int port, const char *name, const char *passwd, const char *video_id, const char *mesh_name);
	~ConnCtrl();

	virtual void RegisterObserver(MainWndCallback* callback);
	virtual bool IsWindow();

	virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video);
	virtual void StopRemoteRenderer();

	virtual void ConnectedPeerResult(bool result);
	virtual void QueueUIThreadCallback(int msg_id, void* data);

	void ThreadCallbackHandle();
	// Creates and shows the main window with the |Connect UI| enabled.
	bool Create();

	// Destroys the window.  When the window is destroyed, it ends the
	// main message loop.
	bool Destroy();

	void CfgCmdProc();

	// callback from PeerConnectionMesh 
	void OnMeshSignFailed();
	void OnMeshSignedIn(int id, const std::string& name, bool connected);  // Called when we're logged on.
	void OnMeshDisconnected(int reason);
	void OnMeshPeerConnected(int id, const std::string& name);
	void OnMeshPeerDisconnected();
  
	// action from upper layer such as meshui 
	void LoginToMeshServer(std::string server, int port);
	void DisconnMeshPeer(int status, int reason);
	void DisconnMeshServer(int cur_status);
	int  ConnectMeshPeer();

	void MeshServerSet(const char* server);
	void AppCmdSet(const char* cmd);
	bool AppCmdWaiting();
	void MeshStatusProc();
	
	// remote rtc server process
	void WebrtcMediaSet(std::string addr, std::string port, std::string port_s);
	void WebrtcServerSet(std::string addr, std::string port, std::string port_s);
	void LoginToWebrtcServer();
	void LogoutFromWebrtcServer();
	void ServerStatusProc();
	void Process();

    // webrtc server conn 
	std::string      webrtc_username_;
	std::string      webrtc_passwd_;
	std::string      video_id_;
	
	int              webrtc_server_status;
	unsigned int     webrtc_conn_interval;
	uint64_t         webrtc_conn_last_time;
	unsigned int     webrtc_login_retry;
	unsigned int     webrtc_login_retry_max;
	int              webrtc_server_cmd_;
	
	std::string      local_sdp_desc_;	
	
    // mesh server conn 	
	MainWndCallback* callback_;
	std::string      mesh_server_;
	int              mesh_port_;
	int              mesh_status;
	std::string      mesh_name_;

//	uint64_t         mesh_conn_last_time;
	std::string      rtc_server_;

	unsigned int     mesh_login_retry_max;
	
	int              cfg_cmd_;
	int              upper_layer_cmd_; // such as mesh ui	
	std::string      args;
	
protected:

	std::unique_ptr<VideoRenderer> remote_renderer_;
    bool  local_render_flag;

	int mesh_local_id_;	
	int mesh_peer_id_;
	int width_;
	int height_;
};

#endif // MAIN_WND_USING

#endif  // __MAIN_CTRL_H_
