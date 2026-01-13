/*
 */
#ifndef MAIN_WND_USING

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstdint>
#include <map>
#include <utility>
#include <mutex>

#include "api/video/i420_buffer.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/video/video_source_interface.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/convert_from.h"

#include "fake_video_render.h"
#include "conn_ctrl.h"
#include "conductor.h"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"
#include "ipc_msg.h"
#include "av_cfg.h"
#include "mesh_call.h"
/////////////////////////////////////////////////////////////////////////////////////////////////
namespace {

	struct UIThreadCallbackData {
		explicit UIThreadCallbackData(MainWndCallback* cb, int id, void* d)
			: callback(cb), msg_id(id), data(d) {}

		MainWndCallback* callback;
		int msg_id;
		void* data;
	};

	bool HandleUIThreadCallback(UIThreadCallbackData *data)
	{
		UIThreadCallbackData* cb_data = reinterpret_cast<UIThreadCallbackData*>(data);
		cb_data->callback->UIThreadCallback(cb_data->msg_id, cb_data->data);
		return false;
	}

}  // namespace

////////////////////////////////////////////////////////////////////////////////////////////
extern std::string media_server_secure_port_default;
extern std::string media_server_port_default;

extern void mesh_video_res_set(int scale_down);

void rtc_server_conn_status_update(int logined);

int  g_wh_limited = 0;

unsigned int  login_retry_interval = 8000;
std::mutex mtxlock;
std::deque<UIThreadCallbackData *> pending_callback_data_; // save all webrtc msg
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct  tag_app_cmd_info {
	int        cmd;
	const char *cmd_desc;
}app_cmd_info_t;	

app_cmd_info_t app_cmd_list[16] = {
	{MESH_LOGIN,   MESH_LOGIN_CMD},
	{MESH_DISCONN, MESH_CALL_STOP_CMD},
	{MESH_CONN,    MESH_CALL_START_CMD},	
	{MESH_LOGOUT,  MESH_LOGOUT_CMD},	
	{MESH_SERVER_SET,  MESH_SERVER_SET_CMD},
	{RTC_SERVER_SET	,  RTC_SERVER_SET_CMD},
	{RTC_MEDIA_SET	,  RTC_MEDIA_SET_CMD},
	{RTC_DISCONN, RTC_DISCONN_CMD},	
	{RTC_LOGIN,   RTC_LOGIN_CMD},	
	{RTC_LOGOUT,  RTC_LOGOUT_CMD},	
	{RTC_HDMI_CALL, RTC_HDMI_CALL_CMD},
	{DJI_STREAM_NAME_SET, DJI_STREAM_NAME_SET_CMD},
	{RTC_DJI_STREAM_STOP, RTC_DJI_STREAM_STOP_CMD},
	{SWITCH_TO_RTC_SERVER, SWITCH_RTC_SERVER_CMD},
	{SWITCH_TO_MESH_SERVER, SWITCH_MESH_SERVER_CMD},
	{-1, NULL}
};

/////////////////////////////////////////////////////////////////////////////////////////////////
//
// ConnCtrl implementation.
//
ConnCtrl::ConnCtrl(const char* server, int port, const char *name, const char *passwd, const char *video_id, const char *mesh_name)
	: callback_(NULL),
	mesh_server_(server),
	mesh_port_(port),
	rtc_server_("0.0.0.0")
{
	width_      = 0;
	height_     = 0;
	mesh_peer_id_  = -1;
	mesh_local_id_ = -1;
	mesh_name_     = std::string(mesh_name);
	
	webrtc_username_     = name;
	webrtc_passwd_       = passwd;
	video_id_			 = video_id;	
	
	webrtc_login_retry   = 0;
	webrtc_conn_interval = login_retry_interval;
	webrtc_server_status = PC_CONN_SERVER_IDLE;
	
	local_sdp_desc_      = "";

	webrtc_server_cmd_   = -1; //CMD_LOGIN; //To start conn webrtc server immediately!
	webrtc_conn_last_time= 0;
	
//	mesh_conn_last_time  = 0;
	mesh_status          = PC_CONN_MESH_IDLE;

	webrtc_login_retry_max = -1;
		
	args = "";
	
	if("127.0.0.1" == mesh_server_)
	{
		mesh_login_retry_max = -1; // max retry conn local server
		upper_layer_cmd_     = -1; //To start conn local server immediately!		
	} else {
		mesh_login_retry_max = 1; // only once
		upper_layer_cmd_     = -1;
	}
	
	cfg_cmd_          = -1;
	local_render_flag = false; 
	
	ipc_msg_send(MESH_UI_MODULE_NAME, RTC_LOGOUT_IND, strlen(RTC_LOGOUT_IND)+1);
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "init: send RTC_LOGOUT_IND fail to meshui ");
	
	rtc_server_conn_status_update(0);
}

ConnCtrl::~ConnCtrl()
{
	rtc_server_conn_status_update(0);
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::MeshServerSet(const char* server)
{
	std::string server_ip(server);
	mesh_server_ = server_ip;

	if("127.0.0.1" == mesh_server_)
	{
		mesh_login_retry_max = -1; // max retry conn local server
	} else {
		mesh_login_retry_max = 1; // only once
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////
bool ConnCtrl::AppCmdWaiting()
{
	static int check_num = 0;

	if( (cfg_cmd_ == -1) && (upper_layer_cmd_ == -1) && (webrtc_server_cmd_ == -1) )
	{		
		return false;
	}

	check_num++;
	if(check_num % 300 == 0)
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "cmd not finished? cfg_cmd_=%d, upper_layer_cmd_=%d, webrtc_server_cmd_=%d", cfg_cmd_, upper_layer_cmd_, webrtc_server_cmd_ );
		exit(0);	//这里设置程序自己退出，由service重新拉起，防止卡死。
	    // cfg_cmd_ = -1;
        // upper_layer_cmd_ = -1;
        // webrtc_server_cmd_ = -1;
	}
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////
//Using a simple str as command. Maybe use json as command in the future.
void ConnCtrl::AppCmdSet(const char* cmd_str)
{
	unsigned  int loop;
	app_cmd_info_t *cmd_info;
	const char *p_src;
	char *p_dst;
	char cmd_desc[64];
	
	memset(cmd_desc, 0, 64);
	p_dst = cmd_desc;
	p_src = cmd_str;
	while(*p_src != ' ')
	{
		*p_dst = *p_src;
		if(*p_src == 0)
			break;
		p_src++;
		p_dst++;
	}

	// skip space 
	while(*p_src == ' ')
	{
		if(*p_src == 0)
			break;
		p_src++;
	}	
	
	if(*p_src != 0)
	{
		args = std::string(p_src);
	}
	
	for(loop=0; ; loop++)
	{
		cmd_info = &app_cmd_list[loop];
		if(cmd_info->cmd == -1)
		{
			break;
		}

		if(0 != strcmp(cmd_desc, cmd_info->cmd_desc))
		{
			continue;
		}

		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "cmd: %s args: %s\n", cmd_info->cmd_desc, args.c_str());		
		if((cmd_info->cmd <= SWITCH_TO_MESH_SERVER) && (cmd_info->cmd >= MESH_SERVER_SET))
		{
			cfg_cmd_ = cmd_info->cmd;
			return;
		}		
		
		if(cmd_info->cmd < RTC_MEDIA_SDP_SEND)
		{
			upper_layer_cmd_   = cmd_info->cmd;
		} else {
			webrtc_server_cmd_ = cmd_info->cmd;
		}

		return;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::RegisterObserver(MainWndCallback* callback) 
{
	callback_ = callback;
	
	// init default params for debug/test
	WebrtcServerSet("10.30.2.8", "8972", "8970");
	WebrtcMediaSet("10.30.2.8", media_server_port_default.c_str(), media_server_secure_port_default.c_str());
}

bool ConnCtrl::IsWindow()
{
	return true; // I am always exist
}

////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video)
{
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "StartRemoteRenderer for video track");
	
	remote_renderer_.reset(new VideoRenderer(this, remote_video));
	local_render_flag = true;
}

void ConnCtrl::StopRemoteRenderer()
{
	if(local_render_flag)
	{
		remote_renderer_.reset();
	}
	local_render_flag = false;
}

////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::ConnectedPeerResult(bool result)
{
	char buf[128];
	
	if(result)
	{
		if(mesh_status == PC_CONN_MESH_CONNECTED)
		{
			return; // do nothing
		}
		
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ConnectedPeerResult] mesh_status = %s webrtc_server_status = %s", mesh_conn_status_string(mesh_status), server_conn_status_string(webrtc_server_status));
		if(mesh_status == PC_CONN_MESH_CONNECTING)
		{
			mesh_status = PC_CONN_MESH_CONNECTED;
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ConnectedPeerResult] mesh_status from PC_CONN_MESH_LOGINED to PC_CONN_MESH_CONNECTED ");
		}
		if(webrtc_server_status == PC_CONN_SERVER_IDLE)
		{
			if(gb28181_server_conn_check())
			{
				av_wh_limit_reset(0); //server link no limit.
			}
			else
			{
				av_wh_limit_reset(1); //mesh link limit av wh 604x352
			}
		}
		else
		{
			av_wh_limit_reset(0); //server link no limit.
		}
		if(webrtc_server_status == PC_CONN_SERVER_IDLE){
			sprintf(buf, "%s:OK", MESH_CALL_CONN_IND);
			ipc_msg_send(MESH_UI_MODULE_NAME, buf, strlen(buf)+1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ConnectedPeerResult]send MESH_CALL_CONN_IND ok to meshui ");
		}
	} else {
		if(mesh_status == PC_CONN_MESH_CONNECTING)
		{
			mesh_status = PC_CONN_MESH_LOGINED; // restore to prev status
		}
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ConnectedPeerResult] mesh_status = %s webrtc_server_status = %s", mesh_conn_status_string(mesh_status), server_conn_status_string(webrtc_server_status));
		
		sprintf(buf, "%s:FAIL", MESH_CALL_CONN_IND);
		ipc_msg_send(MESH_UI_MODULE_NAME, buf, strlen(buf)+1);
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ConnectedPeerResult]send MESH_CALL_CONN_IND fail to meshui ");
	}
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::QueueUIThreadCallback(int msg_id, void* data)
{
	UIThreadCallbackData * msg = new UIThreadCallbackData(callback_, msg_id, data);
	if (msg)
	{
		mtxlock.lock();
		pending_callback_data_.push_back(msg);
		mtxlock.unlock();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::ThreadCallbackHandle()
{
	std::string* sdp;
	
	if (pending_callback_data_.empty())
	{
		return;
	}

	mtxlock.lock();
	UIThreadCallbackData *msg = pending_callback_data_.front();
	if (!msg)
	{
		mtxlock.unlock();
		return;
	}

	pending_callback_data_.pop_front();
	mtxlock.unlock();

	switch(msg->msg_id)
	{
	case WEBRTC_SERVER_LOGINED:
	    if(webrtc_server_cmd_ != -1)
		{
			// ? process too slow. 
		}
		webrtc_server_cmd_ = RTC_LOGIN_OK;
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ThreadCallbackHandle]send webrtc_server_cmd_  RTC_LOGIN_OK");	
	break;
	
	case WEBRTC_SERVER_LOGOUT:
	    if(webrtc_server_cmd_ != -1)
		{
			// ? process too slow. 
		}
		webrtc_server_cmd_ = RTC_LOGOUT_MSG;	
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ThreadCallbackHandle]send webrtc_server_cmd_  RTC_LOGOUT_MSG");	
	break;	
	
	case WEBRTC_SERVER_CALL:
		callback_->ConnectToRtcServer();  // rtc media server connect 
	break;
	
	case WEBRTC_SERVER_HANGUP:
		webrtc_server_cmd_ = RTC_DISCONN;
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[ThreadCallbackHandle]send webrtc_server_cmd_  RTC_DISCONN");	
	break;
	
	case WEBRTC_SERVER_MESSAGE_SEND:
		sdp = reinterpret_cast<std::string*>(msg->data);
		if(sdp)
		{
			local_sdp_desc_ = *sdp;
			webrtc_server_cmd_ = RTC_MEDIA_SDP_SEND;
		}
	break;
	
	default:
		HandleUIThreadCallback(msg);
	break;
	}		
	
	delete msg; 
}
////////////////////////////////////////////////////////////////////////////////////////////
bool ConnCtrl::Destroy()
{
	callback_->Close();   // ??? check all resource to free
	return true;
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::OnMeshSignFailed()
{
	mesh_status = PC_CONN_MESH_IDLE;
	//server_conn_error = 1;
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[OnMeshSignFailed]mesh_status switch to  PC_CONN_MESH_IDLE \n");	
	
	ipc_msg_send(MESH_UI_MODULE_NAME, MESH_LOGIN_FAIL, strlen(MESH_LOGIN_FAIL)+1);	
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[OnMeshSignFailed]send MESH_LOGIN_FAIL to meshui ");
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::OnMeshSignedIn(int id, const std::string& name, bool connected)
{
	if(name == mesh_name_)
	{
		mesh_status    = PC_CONN_MESH_LOGINED;
		mesh_local_id_ = id;
		ipc_msg_send(MESH_UI_MODULE_NAME, MESH_LOGIN_IND, strlen(MESH_LOGIN_IND)+1);	
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "switch to  PC_CONN_MESH_LOGINED local mesh dev info: name=%s , id=%d connected=%d", name.c_str(), id, connected);		
	} else {
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, " Not update local mesh dev: name=%s , id=%d but mesh_name_=%s ", name.c_str(), id, mesh_name_.c_str());
	}		
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::OnMeshDisconnected(int reason)
{
	if(mesh_status == PC_CONN_MESH_LOGINING)
	{
		mesh_status = PC_CONN_MESH_IDLE;
		if(reason == MESH_EVENT_PEER_BUSY) 
		{
			ipc_msg_send(MESH_UI_MODULE_NAME, MESH_LOGIN_BUSY, strlen(MESH_LOGIN_BUSY)+1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[OnMeshDisconnected]send MESH_LOGIN_BUSY to meshui ");			
		} else {
			ipc_msg_send(MESH_UI_MODULE_NAME, MESH_LOGIN_FAIL, strlen(MESH_LOGIN_FAIL)+1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[OnMeshDisconnected]send MESH_LOGIN_FAIL to meshui ");			
		}
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "mesh_status switch to  PC_CONN_MESH_IDLE reason=%d\n", reason);	
	}	
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::OnMeshPeerConnected(int id, const std::string& name)
{
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "update peer info: mesh_peer_id_=%d name=%s & local mesh_name_=%s\n", id, name.c_str(), mesh_name_.c_str());

	if(name == mesh_name_)
	{
		return; // peer name can not same as local mesh name 
	}

	mesh_peer_id_ = id;
	size_t pos    = name.find("@"); // ip@name
	if (pos == std::string::npos) 
	{
		return; // not a valid mesh name ?
	}

	std::string ip = name.substr(0, pos);
	FILE *fp = fopen(MESH_PEER_ADDR_FILE, "w+");
	if(fp)
	{
		fwrite(ip.c_str(), 1, ip.length(), fp);
		fclose(fp);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::OnMeshPeerDisconnected()
{
	if(PC_CONN_MESH_CONNECTED == mesh_status)
	{
		mesh_status = PC_CONN_MESH_LOGINED;		
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "OnMeshPeerDisconnected: disconn by peer ... switch to PC_CONN_MESH_LOGINED mesh_peer_id_=%d\n", mesh_peer_id_);	
	}
	else 
	{
		mesh_peer_id_ = -1;
		mesh_status   = PC_CONN_MESH_LOGINING;		
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "OnMeshPeerDisconnected: disconn by peer ... switch to PC_CONN_MESH_LOGINING mesh_peer_id_=%d\n", mesh_peer_id_);	
	}
	
	ipc_msg_send(MESH_UI_MODULE_NAME, MESH_CALL_DISCONN_IND, strlen(MESH_CALL_DISCONN_IND)+1);
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[OnMeshPeerDisconnected]send MESH_CALL_DISCONN_IND to meshui ");
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::LoginToMeshServer(std::string server_ip, int server_port)
{
	mesh_server_ = server_ip;
	mesh_port_   = server_port;
	
	callback_->StartLogin(mesh_server_, mesh_port_, mesh_name_.c_str());
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::DisconnMeshPeer(int status, int reason)
{
	if(status == PC_CONN_SERVER_LOGINED)
	{
		ipc_msg_send(MESH_UI_MODULE_NAME, RTC_DISCONN_IND, strlen(RTC_DISCONN_IND)+1);
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[DisconnMeshPeer] status=%d reason=%d : send RTC_DISCONN_IND to meshui ", status, reason);		
	} else {
		if((status == PC_CONN_MESH_CONNECTED) && (reason == MESH_LOGOUT))
		{
			ipc_msg_send(MESH_UI_MODULE_NAME, MESH_LOGOUT_IND, strlen(MESH_LOGOUT_IND)+1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[DisconnMeshPeer] status=%d reason=%d : send MESH_LOGOUT_IND to meshui ", status, reason);			
		} else {
			ipc_msg_send(MESH_UI_MODULE_NAME, MESH_CALL_DISCONN_IND, strlen(MESH_CALL_DISCONN_IND)+1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[DisconnMeshPeer] status=%d reason=%d : send MESH_CALL_DISCONN_IND to meshui ", status, reason);
		}
	}
	
	callback_->DisconnectFromCurrentPeer(status);	
}
//////////////////////////////////////////////////////////////////////////////////////////// 
void ConnCtrl::DisconnMeshServer(int cur_status)
{
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DisconnMeshServer  starting... on %s status\n", mesh_conn_status_string(cur_status));	
	callback_->DisconnectFromServer();
}
////////////////////////////////////////////////////////////////////////////////////////////
int ConnCtrl::ConnectMeshPeer()
{
	if (mesh_peer_id_ != -1)
	{
		callback_->ConnectToPeer(mesh_peer_id_);
		return 1;
	}
	
	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "ConnectMeshPeer failed.  mesh_peer_id_=%d \n", mesh_peer_id_);
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::CfgCmdProc()
{
	int cmd;
	size_t end;
	std::string port_s, ip_str;

	if(cfg_cmd_ == -1)	
	{
		return;
	}
	
	// cmd process
	cmd = cfg_cmd_;
	cfg_cmd_ = -1;
	
	switch(cmd)
	{
	case SWITCH_TO_RTC_SERVER:
		end = args.find(":");
		if (end == std::string::npos) 
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_RTC_SERVER: ip addr without port !!!! %s ", args.c_str());
			break;				 
		}

		port_s = std::to_string(8970); // default secure port
		ip_str = args.substr(0, end);
		if(!is_ipv4_addr(ip_str.c_str()))
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC_SERVER_SET: ip addr error! %s ", ip_str.c_str());
			break;	
		}
		
		av_tmp_rtc_server_save(args.c_str()); // save rtc server info for restore lately in some rare case
		mesh_server_ = "";
		if(PC_CONN_MESH_IDLE != mesh_status) 
		{
			upper_layer_cmd_ = MESH_LOGOUT; // logout from  mesh server
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_RTC_SERVER: MESH_LOGOUT  ");
			break;
		}
		
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_RTC_SERVER: mesh_status=%d %s webrtc_server_status=%d %s", \
							mesh_status, mesh_conn_status_string(mesh_status), webrtc_server_status, server_conn_status_string(webrtc_server_status));
		if(rtc_server_ != args) // rtc server change
		{
			rtc_server_ = args;
			callback_->WebrtcServerSet(ip_str, args.substr(end+1), port_s);
			if(PC_CONN_SERVER_IDLE != webrtc_server_status)
			{
				webrtc_server_cmd_ = RTC_LOGOUT; // logout from prev rtc server
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_RTC_SERVER: RTC_LOGOUT  ");
			}
			break;
		} 	
		
		if(webrtc_server_status == PC_CONN_SERVER_IDLE)
		{
			webrtc_server_cmd_ = RTC_LOGIN; // login into new rtc server
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_RTC_SERVER: RTC_LOGIN  ");
		}
	break;

	case SWITCH_TO_MESH_SERVER:
		if(!is_ipv4_addr(args.c_str()))
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_MESH_SERVER: ip addr error! %s ", args.c_str());
			break;				 
		}
 
		rtc_server_ = "";
		if(webrtc_server_status != PC_CONN_SERVER_IDLE)
		{
			mesh_server_  = args;
			webrtc_server_cmd_ = RTC_LOGOUT; // logout from prev rtc server
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_MESH_SERVER: RTC_LOGOUT. (mesh_server_=%s)  ", mesh_server_.c_str());
			break;
		}
		
		if(mesh_server_ != args) // mesh server change
		{
			mesh_server_  = args;
			if(PC_CONN_MESH_IDLE != mesh_status) 
			{
				upper_layer_cmd_ = MESH_LOGOUT; // logout from prev mesh server
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_MESH_SERVER: MESH_LOGOUT (mesh_server_=%s)  ", mesh_server_.c_str());
			}
			break;
		} 
		
		if(PC_CONN_MESH_IDLE == mesh_status) 
		{
			upper_layer_cmd_ = MESH_LOGIN; // login into new mesh server
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "SWITCH_TO_MESH_SERVER: MESH_LOGIN (mesh_server_=%s)  ", mesh_server_.c_str());
		}			
	break;
		
	case MESH_SERVER_SET:
		if(!is_ipv4_addr(args.c_str()))
		{
			 MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "MESH_SERVER_SET: ip addr error! %s ", args.c_str());
			 break;
		}

		if(mesh_status != PC_CONN_MESH_IDLE)
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "make sure excute MESH_LOGOUT before change server !!!!");
		} else {
			mesh_server_ = args;		
		}			
		av_tmp_rtc_server_save(""); // clean rtc server info
	break;
		
	case RTC_SERVER_SET:
		end = args.find(":");
		if (end == std::string::npos) 
		{
			break;
		}

		av_tmp_rtc_server_save(args.c_str()); // save rtc server info for restore lately in some rare case
				
		port_s = std::to_string(8970); // default secure port
		ip_str = args.substr(0, end);
		if(!is_ipv4_addr(ip_str.c_str()))
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC_SERVER_SET: ip addr error! %s ", ip_str.c_str());
			break;
		}

		callback_->WebrtcServerSet(ip_str, args.substr(end+1), port_s);
	break;
	
	case RTC_MEDIA_SET:
		end = args.find(":");
		if (end != std::string::npos) 
		{
			callback_->WebrtcMediaSet(args.substr(0, end), args.substr(end+1), media_server_secure_port_default);				
		}
	break;

	default:
	break;
	}
	
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::MeshStatusProc()
{
	MainWndCallback *callback;
	int ret;

	if(upper_layer_cmd_ == -1)
	{			
		return;
	}

	callback =  callback_;
	if(!callback)
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "callback_ is null and discard cmd %d ", upper_layer_cmd_);
		upper_layer_cmd_ = -1;	
		return; // not ready for peerconnnection
	}

	switch (mesh_status)
	{
	case PC_CONN_MESH_IDLE:
		if (upper_layer_cmd_ == MESH_LOGIN)
		{
			mesh_video_res_set(1);
			g_wh_limited = 1;
			//				mesh_conn_last_time = sys_time_get();
			LoginToMeshServer(mesh_server_, mesh_port_);
			mesh_status = PC_CONN_MESH_LOGINING;
		}
		else if (upper_layer_cmd_ == MESH_LOGOUT)
		{
			// do nothing
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_IDLE] logout in idle status:  \n");
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_IDLE]unsupport cmd in this status: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;

	case PC_CONN_MESH_LOGINING:
		// PeerConnectionMesh will relogin if failed
		if (upper_layer_cmd_ == MESH_LOGOUT)
		{
			// If connected to webrtc server, stop connecting local/remote mesh server		
			mesh_status = PC_CONN_MESH_IDLE;
			DisconnMeshServer(PC_CONN_MESH_LOGINING); // release all conn resource
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_LOGINING] stop loging to mesh server \n");
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_LOGINING]unsupport cmd: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;

	case PC_CONN_MESH_LOGINED:
		if (upper_layer_cmd_ == MESH_LOGOUT)
		{
			DisconnMeshServer(PC_CONN_MESH_LOGINED);
			mesh_status = PC_CONN_MESH_IDLE;
		}
		else if (upper_layer_cmd_ == MESH_CONN)
		{
			ret = ConnectMeshPeer();
			if (ret > 0)
			{
				mesh_status = PC_CONN_MESH_CONNECTING;
			}
			else {
				// Peer is not reachable, to send MESH_CALL_REJECT_IND rsp to upper layer?
				ipc_msg_send(MESH_UI_MODULE_NAME, MESH_CALL_FAIL_IND, strlen(MESH_CALL_FAIL_IND) + 1);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_LOGINED]send MESH_CALL_FAIL_IND to meshui ");
			}
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_LOGINED]unsupport cmd: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;

	case PC_CONN_MESH_CONNECTING:
		if (upper_layer_cmd_ == MESH_DISCONN)
		{
			DisconnMeshPeer(PC_CONN_MESH_CONNECTING, MESH_DISCONN);
			mesh_status = PC_CONN_MESH_LOGINED;
		}
		else if (upper_layer_cmd_ == MESH_LOGOUT)
		{
			DisconnMeshPeer(PC_CONN_MESH_CONNECTING, MESH_LOGOUT);
			DisconnMeshServer(PC_CONN_MESH_CONNECTING);
			mesh_status = PC_CONN_MESH_IDLE;
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_CONNECTING]unsupport cmd: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;

	case PC_CONN_MESH_CONNECTED:
		if (upper_layer_cmd_ == MESH_DISCONN)
		{
			av_wh_limit_reset(0);
			DisconnMeshPeer(PC_CONN_MESH_CONNECTED, MESH_DISCONN);
			mesh_status = PC_CONN_MESH_LOGINED;
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DisconnMeshPeer starting... switch status to %d %s\n", mesh_status, mesh_conn_status_string(mesh_status));
		}
		else if (upper_layer_cmd_ == MESH_LOGOUT)
		{
			av_wh_limit_reset(0);
			DisconnMeshPeer(PC_CONN_MESH_CONNECTED, MESH_LOGOUT);
			DisconnMeshServer(PC_CONN_MESH_CONNECTED);
			mesh_status = PC_CONN_MESH_IDLE;
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_CONNECTED]unsupport cmd: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;
	
	/*
	case PC_CONN_MESH_DISCONNECTING:
		if( upper_layer_cmd_ == MESH_LOGOUT)
		{
			 DisconnMeshServer();
			 mesh_status = PC_CONN_MESH_IDLE;
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_MESH_CONNECTED]unsupport cmd: %d \n",  upper_layer_cmd_);
		}
	break;
	*/

	default:
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[unknown status]unsupport cmd: %d %s\n", upper_layer_cmd_, cmd_string(upper_layer_cmd_));
	break;
	}
	
	upper_layer_cmd_ = -1;	
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::WebrtcServerSet(std::string addr, std::string port, std::string port_s)
{	
	callback_->WebrtcServerSet(addr, port, port_s);	
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::WebrtcMediaSet(std::string addr, std::string port, std::string port_s)
{
	callback_->WebrtcMediaSet(addr, port, port_s);		
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::LogoutFromWebrtcServer()
{	
	callback_->StartWebrtcLogout();
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::LoginToWebrtcServer()
{
	callback_->StartWebrtcLogin(webrtc_username_, webrtc_passwd_, video_id_);
}
////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::ServerStatusProc()
{
	uint64_t now;
	int      inner_server_status = 0;

	Conductor* conductor = dynamic_cast<Conductor*>(callback_);
	
	if( webrtc_server_cmd_ == -1)
	{
		if( webrtc_server_status != PC_CONN_SERVER_LOGINING)
		{
			return;
		}

		webrtc_login_retry--;
		if( webrtc_login_retry == 0)
		{
			return;
		}
			
		now = sys_time_get();
		if((now -  webrtc_conn_last_time) >  webrtc_conn_interval)
		{
			LoginToWebrtcServer();
			webrtc_conn_last_time = now;
		}
		
		return;
	}
	
	// cmd process
	switch (webrtc_server_status)
	{
	case PC_CONN_SERVER_IDLE:
		if (webrtc_server_cmd_ == RTC_LOGIN)
		{
			mesh_video_res_set(0);
			conductor->SetServerType(0);
#ifdef MESH_DJI
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "MESHRTC FOR DJI: NOT LOGIN SERVER.\n");
#else
			webrtc_login_retry = webrtc_login_retry_max;
			LoginToWebrtcServer();

			webrtc_server_status = PC_CONN_SERVER_LOGINING;
			webrtc_conn_last_time = sys_time_get();
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "switch to status: PC_CONN_SERVER_LOGINING \n");
#endif  //MESH_DJI
		}
		else if (webrtc_server_cmd_ == RTC_HDMI_CALL)
		{
			webrtc_server_status = PC_CONN_SERVER_LOGINED;
			conductor->GetInnerServerStatus(); // wangyun: check server status
			inner_server_status = conductor->GetServerStatusFlag();
			if (inner_server_status == 1) {
				conductor->SetServerType(1);
#ifndef MESH_DJI
				conductor->SetStreamID(webrtc_username_);
#endif
				mesh_video_res_set(0);
				callback_->ConnectToRtcServer();  // rtc media server connect
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success.\n");
			}
			else if(inner_server_status == 0)
			{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: Server not Ready!\n");
			}
			else if(inner_server_status == -1)
			{
				callback_->DisconnectFromCurrentPeer(1);
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: HEARTBEAT TO ZLM SERVER ERROR!\n");
				system("echo 0 > /tmp/rtc_server_conn");
			}
			else{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success: Already Connected to Server!\n");
			}
		}
		else if(webrtc_server_cmd_ == DJI_STREAM_NAME_SET){
			if (!args.empty()) {
				std::string stream_id = args;
				size_t pos = stream_id.find('_');
				if (pos != std::string::npos) {
					stream_id = stream_id.substr(0, pos); 
				}
				conductor->SetAppName(stream_id);
				conductor->SetStreamID(args);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: set stream id to %s", args.c_str());
			} else {
				conductor->SetStreamID(webrtc_username_); // 兼容原有逻辑
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: args empty, set stream id to webrtc_username_ %s", webrtc_username_.c_str());
			}			
		}
		else if(webrtc_server_cmd_ == RTC_DJI_STREAM_STOP){
			conductor->OnClientHangup();
		}
		else {
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_IDLE]unsupport cmd: %d \n", webrtc_server_cmd_);
		}
	break;

	case PC_CONN_SERVER_LOGINING:
		if (webrtc_server_cmd_ == RTC_LOGIN_OK)
		{
			// disconn mesh connection 
			conductor->SetServerType(0);
			upper_layer_cmd_ = MESH_LOGOUT; // disconn mesh conn
			webrtc_server_status = PC_CONN_SERVER_LOGINED;
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINING]change to : PC_CONN_SERVER_LOGINED \n");
			rtc_server_conn_status_update(1);

			ipc_msg_send(MESH_UI_MODULE_NAME, RTC_LOGIN_IND, strlen(RTC_LOGIN_IND) + 1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINING]send RTC_LOGIN_IND to meshui ");
		}
		else if (webrtc_server_cmd_ == RTC_LOGOUT)
		{
			upper_layer_cmd_ = MESH_LOGIN; // start connect to mesh server
			webrtc_server_status = PC_CONN_SERVER_IDLE;
			LogoutFromWebrtcServer();
		}
		else if (webrtc_server_cmd_ == RTC_HDMI_CALL)
		{
			webrtc_server_status = PC_CONN_SERVER_LOGINED;
			conductor->GetInnerServerStatus(); // wangyun: check server status
			inner_server_status = conductor->GetServerStatusFlag();
			if (inner_server_status == 1) {
				conductor->SetServerType(1);
#ifndef MESH_DJI
				conductor->SetStreamID(webrtc_username_);
#endif
				mesh_video_res_set(0);
				callback_->ConnectToRtcServer();  // rtc media server connect
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success.\n");
        	}
			else if(inner_server_status == 0)
			{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: Server not Ready!\n");
			}
			else if(inner_server_status == -1)
			{
				callback_->DisconnectFromCurrentPeer(1);
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: Stream ERROR!\n");
			}
			else{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success: Already Connected to Server!\n");
			}
		}
		else if(webrtc_server_cmd_ == DJI_STREAM_NAME_SET){
			if (!args.empty()) {
				std::string stream_id = args;
				size_t pos = stream_id.find('_');
				if (pos != std::string::npos) {
					stream_id = stream_id.substr(0, pos); 
				}
				conductor->SetAppName(stream_id);
				conductor->SetStreamID(args);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: set stream id to %s", args.c_str());
			} else {
    		    conductor->SetStreamID(webrtc_username_); // 兼容原有逻辑
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: args empty, set stream id to webrtc_username_ %s", webrtc_username_.c_str());
			}			
		}
		else if(webrtc_server_cmd_ == RTC_DJI_STREAM_STOP)
		{
			conductor->OnClientHangup();
		}
		else
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINING]unsupport : upper_layer_cmd_=%d webrtc_server_cmd_=%d %s\n", upper_layer_cmd_, webrtc_server_cmd_, cmd_string(upper_layer_cmd_));
		}
	break;

	case PC_CONN_SERVER_LOGINED:
		if (webrtc_server_cmd_ == RTC_LOGOUT)
		{
			DisconnMeshPeer(PC_CONN_SERVER_LOGINED, RTC_LOGOUT); // maybe in connected status

			upper_layer_cmd_ = MESH_LOGIN; // start connect to mesh server
			webrtc_server_status = PC_CONN_SERVER_IDLE;
			LogoutFromWebrtcServer();
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINED]change to : PC_CONN_SERVER_IDLE \n");
			rtc_server_conn_status_update(0);

			ipc_msg_send(MESH_UI_MODULE_NAME, RTC_LOGOUT_IND, strlen(RTC_LOGOUT_IND) + 1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINED]send RTC_LOGOUT_IND to meshui ");
		}
		else if (webrtc_server_cmd_ == RTC_MEDIA_SDP_SEND)
		{
			if ((local_sdp_desc_ != ""))
			{
				// To send sdp msg to rtc media server periodly, because we don't know when it's offline or online
				callback_->SendWebrtcSdp(local_sdp_desc_);
			}
		}
		else if (webrtc_server_cmd_ == RTC_DISCONN)
		{
			rtc_server_conn_status_update(1); // refresh status
			DisconnMeshPeer(PC_CONN_SERVER_LOGINED, RTC_DISCONN);
		}
		else if (webrtc_server_cmd_ == RTC_LOGOUT_MSG)
		{
			webrtc_server_status = PC_CONN_SERVER_LOGINING;
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINED]change to : PC_CONN_SERVER_LOGINING \n");
			rtc_server_conn_status_update(0);
		}
		else if (webrtc_server_cmd_ == RTC_HDMI_CALL)
		{
			conductor->GetInnerServerStatus(); // wangyun: check server status
			inner_server_status = conductor->GetServerStatusFlag();
			if (inner_server_status == 1) {
				conductor->SetServerType(1);
#ifndef MESH_DJI
				conductor->SetStreamID(webrtc_username_);
#endif
				mesh_video_res_set(0);
				callback_->ConnectToRtcServer();  // rtc media server connect
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success.\n");
			}
			else if(inner_server_status == 0)
			{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: Server not Ready!\n");
			}
			else if(inner_server_status == -1)
			{
				callback_->DisconnectFromCurrentPeer(1);
				conductor->SetServerStatusFlag(0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Failed: Stream ERROR!\n");
			}
			else{
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "RTC STREAM CALL Success: Already Connected to Server!\n");
			}
		}
		else if(webrtc_server_cmd_ == DJI_STREAM_NAME_SET){
			if (!args.empty()) {
				std::string stream_id = args;
				size_t pos = stream_id.find('_');
				if (pos != std::string::npos) {
					stream_id = stream_id.substr(0, pos); 
				}
				conductor->SetAppName(stream_id);
				conductor->SetStreamID(args);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: set stream id to %s", args.c_str());
			} else {
    		    conductor->SetStreamID(webrtc_username_); // 兼容原有逻辑
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "DJI STREAM NAME SET: args empty, set stream id to webrtc_username_ %s", webrtc_username_.c_str());
			}			
		}
		else if(webrtc_server_cmd_ == RTC_DJI_STREAM_STOP)
		{
			conductor->OnClientHangup();
		}
		else if (upper_layer_cmd_ == MESH_CONN)
		{
			// if mesh device connected with rtc server, mesh call is not allowed.
			ipc_msg_send(MESH_UI_MODULE_NAME, MESH_CALL_REJECT_IND, strlen(MESH_CALL_REJECT_IND) + 1);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PC_CONN_SERVER_LOGINED]send MESH_CALL_REJECT_IND to meshui ");
		}
	break;

	case PC_CONN_SERVER_CONNECTED:
	default:
		// do nothing
	break;
	}

	webrtc_server_cmd_ = -1;	
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void ConnCtrl::Process()
{
	// callback msg handle
	ThreadCallbackHandle();		
	
	CfgCmdProc();
	
	// get msg from upper layer such as MeshUI
	MeshStatusProc();	
	
	ServerStatusProc(); // shangtao-- 
}
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct tag_id_str_map {
	int         id;
	const char *id_str;
}id_str_map_t;

id_str_map_t cmd_str_map[32] = {
	{MESH_LOGIN,    "MESH_LOGIN"},
	{MESH_CONN,     "MESH_CONN"},
	{MESH_DISCONN,  "MESH_DISCONN"},
	{MESH_LOGOUT,   "MESH_LOGOUT"},
	
	{RTC_MEDIA_SDP_SEND, "RTC_MEDIA_SDP_SEND"},
	{RTC_DISCONN,        "RTC_DISCONN"},
	{RTC_LOGIN,          "RTC_LOGIN"},
	{RTC_HDMI_CALL,      "RTC_HDMI_CALL"},
	{RTC_LOGIN_OK,       "RTC_LOGIN_OK"},
	{RTC_LOGOUT,         "RTC_LOGOUT"},	

	{DJI_STREAM_NAME_SET,  "DJI_STREAM_NAME_SET"},
	{RTC_DJI_STREAM_STOP,  "RTC_DJI_STREAM_STOP"},
	
	{MESH_SERVER_SET,       "MESH_SERVER_SET"},
	{RTC_SERVER_SET,        "RTC_SERVER_SET"},	
	{RTC_MEDIA_SET,         "RTC_MEDIA_SET"},
	{SWITCH_TO_RTC_SERVER,  "SWITCH_TO_RTC_SERVER"},	
	{SWITCH_TO_MESH_SERVER, "SWITCH_TO_MESH_SERVER"},
	
	{-1, "no_cmd"},		
};

const char *cmd_string(int cmd)
{
	if((cmd < MESH_LOGIN) || (cmd > SWITCH_TO_MESH_SERVER))
	{
		return "no_cmd_map_str";
	}
	
	return cmd_str_map[cmd].id_str;
}
////////////////////////////////////////////////////////////////////////////////////////////
id_str_map_t mesh_stauts_str_map[8] = {
	{PC_CONN_MESH_IDLE,        "PC_CONN_MESH_IDLE"},
	{PC_CONN_MESH_LOGINING,    "PC_CONN_MESH_LOGINING"},
	{PC_CONN_MESH_LOGINED,     "PC_CONN_MESH_LOGINED"},
	{PC_CONN_MESH_CONNECTING,  "PC_CONN_MESH_CONNECTING"},
	{PC_CONN_MESH_CONNECTED,   "PC_CONN_MESH_CONNECTED"},
	{PC_CONN_MESH_DISCONNECTING, "PC_CONN_MESH_DISCONNECTING"},
	
	{-1, "no_cmd"},		
};

const char *mesh_conn_status_string(int status)
{
	if((status < PC_CONN_MESH_IDLE) || (status > PC_CONN_MESH_DISCONNECTING))
	{
		return "no_mesh_conn_map_str";
	}
	
	return mesh_stauts_str_map[status - PC_CONN_MESH_IDLE].id_str;
}
////////////////////////////////////////////////////////////////////////////////////////////
id_str_map_t server_stauts_str_map[8] = {
	{PC_CONN_SERVER_IDLE,  "PC_CONN_SERVER_IDLE"},
	{PC_CONN_SERVER_LOGINING,  "PC_CONN_SERVER_LOGINING"},
	{PC_CONN_SERVER_LOGINED,   "PC_CONN_SERVER_LOGINED"},
	{PC_CONN_SERVER_CONNECTED, "PC_CONN_SERVER_CONNECTED"},
	
	{-1, "no_cmd"},			
};
	
const char *server_conn_status_string(int status)
{
	if((status < PC_CONN_SERVER_IDLE) || (status > PC_CONN_SERVER_CONNECTED))
	{
		return "no_server_conn_map_str";
	}
	
	return server_stauts_str_map[status - PC_CONN_SERVER_IDLE].id_str;
}
////////////////////////////////////////////////////////////////////////////////////////////
void rtc_server_conn_status_update(int logined)
{
	FILE *fp;
	char buf[32];
	int  ret;

	fp = fopen(RTC_SERVER_CONN_STATUS_FILE, "r+");
	if(!fp)
	{
		fp = fopen(RTC_SERVER_CONN_STATUS_FILE, "w+");
		if(!fp)
		{
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_ERR, "open  file %s failed \n", RTC_SERVER_CONN_STATUS_FILE);
			return ;
		}
	}
	
	memset(buf, 0, 32);
	ret = fread(buf, 1, 32, fp);
	if(ret > 0)
	{
		ret = atoi(buf);
		if(ret == logined)
		{
			fclose(fp);
			return; // not changed
		}
	}

	memset(buf, 0, 32);
	sprintf(buf, "%d", logined);
	fseek(fp, 0, SEEK_SET);
	ret = fwrite(buf, 1, strlen(buf), fp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_ERR, "write %d  to file %s failed. errno=%d %s\n", logined, RTC_SERVER_CONN_STATUS_FILE, errno, strerror(errno));
	}
	fclose(fp);

	MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "write rtc server conn status %d to file %s \n", logined, RTC_SERVER_CONN_STATUS_FILE);
	
	return;
}

#endif // MAIN_WND_USING
