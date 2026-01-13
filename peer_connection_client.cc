/*
 */
#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"
#include "peer_connection_client.h"

#include "comm/http/http_client.h"
#include "comm/websocket/websocket_client.h"

#include "cjson.h"
#include "json_codec.h"

#include "cfg.h"
#include "av_api.h"
#include "av_cfg.h"
#include "av_conn.h"
#include "video/video_data_recv.h"

#include "cfg_28181.h"

#include "ipc_msg.h"

extern unsigned int  login_retry_interval;
extern void rtc_server_conn_status_update(int logined);
//////////////////////////////////////////////////////////////////////////////////
std::string https_url_head = "https://";
std::string wss_url_head   = "wss://";

std::string http_url_head  = "http://";
std::string ws_url_head    = "ws://";	

std::string media_server_secure_port_default = "10443";
std::string media_server_port_default        = "6080";

static unsigned int max_frag_num  = 3;
static bool https_using           = false; // http is usd for debug

static unsigned int heartbeat_lost_max = 2;
///////////////////////////////////////////////////////////////////////
std::string webrtc_api_path    = "/index/api/webrtc?";
std::string stream_close_path  = "/index/api/close_stream?";
std::string webrtc_secret      = "tzaaghfRyvwtQ7STrA1ag8K1xtThSk9v";

std::string webrtc_logout_path = "/webrtc/login/exit/";
std::string http_login_path    = "/admin/login";

//URL		webrtc/config/setConfig/{memberId}    //terminal id
std::string http_cfg_set_path  = "/webrtc/config/setConfig/"; // not used here
//URL		webrtc/config/report/{memberId}    //终端id
std::string http_cfg_chg_path  = "/webrtc/config/report/";

std::string http_28181_cfg_path = "/webrtc/config/gb28181/";

std::string capability_report_path = "/sys/sendComputerMessage";
///////////////////////////////////////////////////////////////////////
std::string webrtc_ptt_set_req   = "devicePttMsg";

std::string webrtc_cfg_set_req   = "deviceConfigMsg";
std::string webrtc_cfg_set_rsp   = "deviceConfigReplyMsg";

std::string webrtc_gb28181_set_req   = "gb28181ConfigMsg";
std::string webrtc_gb28181_set_rsp   = "gb28181ConfigReplyMsg";

std::string webrtc_cfg_query_req = "deviceConfigQueryMsg";
std::string webrtc_cfg_query_rsp = "deviceConfigQueryReplyMsg";

std::string webrtc_enterAlarmArea_req = "enterAlarmAreaWarningMsg";
std::string webrtc_leaveAlarmArea_req = "leaveAlarmAreaWarningMsg";

std::string webrtc_notify_offline = "offlineMsg";
std::string webrtc_notify_online  = "onlineMsg";
std::string webrtc_ptt_switch     = "pttSwitchMsg";

std::string webrtc_audio_call  = "callAudioMsg";
std::string webrtc_audio_close = "closeCallAudioMsg";
std::string webrtc_video_call  = "callvideomsg";
std::string webrtc_video_close = "closePlayVideoMsg";
std::string webrtc_audio_output= "audioOutputMsg";
std::string webrtc_fluent_first= "fluentFirstModeMsg";

av_params_t    av_params;
av_audio_cfg_t av_audio_cfg;

gb_28181_cfg_t gb_28181_cfg;
///////////////////////////////////////////////////////////////////////
std::string play_type       = "play";
std::string push_type       = "push";
//////////////////////////////////////////////////////////////////////////////////
PeerConnectionClient::PeerConnectionClient(bool only_audio)
	: callback_(NULL)
{
	video_enable_  = true; 
	if(only_audio)
	{
		video_enable_  = false; 
	}
	audio_enable_  = true;
	call_event_id_ = "";	
	cfg_event_id_  = "";
	
	sdp_msg_       = "";
	login_ok       = false;
	conn_starting  = false;
	http_id_       = -1;
	websocket_id_  = -1;
	websocket_heartbeat_interval_ = 3 * 1000; // 10 seconds
	websocket_heartbeat_time      = 0;
	websocket_heartbeat_lost_     = 0;
	heartbeat_msg_id_             = "";
	
	create_id_      = 0;
	ws_data_count_  = 0;
	websocket_data_ = "";			

	http_data_      = "";	
	stream_type_    = "mesh_t";
	peer_server_type_  = 0; // 0: WVP, 1: XV9100
	peer_server_status_ = 0; // 0: not ready, 1: ready
	peer_server_check_times_ = 0;
		
	cfg_report_flag_ = 0;
	gb28181_report_flag_ = 0;
	
	if(https_using)
	{
		https_url_head       = "https://";
		wss_url_head         = "wss://";
		
		media_server_port_   = media_server_port_default;
		media_server_port_s_ = media_server_secure_port_default;
		
	} else {
		
		https_url_head       = "http://";
		wss_url_head         = "ws://";
		media_server_port_   = media_server_port_default;
		media_server_port_s_ = media_server_port_default;
		
		http_url_head        = "http://";
		ws_url_head          = "ws://";			
	}

#if 0	
	// self test
	std::string cfg_event_id_ = "x_c_z";
	WebrtcCfgQueryRsp(cfg_event_id_, 1, 1, 1, 1);
	HttpCfgReport();
#endif	

}
//////////////////////////////////////////////////////////////////////////////////
PeerConnectionClient::~PeerConnectionClient() 
{
    if(http_id_ != -1)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] [~PeerConnectionClient] stop current http conn: http_id_=%d", http_id_);
		http_thread_stop(http_id_);
	}
    if(websocket_id_ != -1)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] [~PeerConnectionClient] stop current websocket conn: websocket_id_=%d", websocket_id_);
		websocket_session_thread_stop(websocket_id_);
	}	
}
//////////////////////////////////////////////////////////////////////////////////
int PeerConnectionClient::Start()
{
	http_id_ = http_thread_start();
		
	return http_id_;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::Process()
{
	HttpProcess();
	
	WebsocketProcess();
	
	if(cfg_report_flag_)
	{
		cfg_report_flag_ = 0;
		HttpCfgReport();
	}

	// if(gb28181_report_flag_)
	// {
	// 	gb28181_report_flag_ = 0;
	// 	GB28181_CfgReport();
	// }
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::SetAppName(const std::string& app_name)
{
	app_id_ = app_name;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::SetStreamID(const std::string& user)
{
	user_name_ = user;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::SetPeerServerType(int server_type) {
	peer_server_type_ = server_type;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::SetPeerServerStatus(int server_status)
{
	peer_server_status_ = server_status;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::GetInnerServerStatus(){
	std::string  req_str;	
	std::string  http_req_url;	
	std::string  post_data;
    if(http_id_ == -1)
	{	
		return;
	}
	peer_server_check_times_ ++;
	// Note: secret is used in wvp & mesht device
	// Get ZLMedia Server Version.
	req_str = "http://" + media_server_address_ + ":" + media_server_port_ + "/index/api/version?" + "secret=" + webrtc_secret;
	
	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] Get ZLMedia Server Version.\n %s ", req_str.c_str());	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::HttpProcess()
{
	char *data;
	int  size;
	cJSON* cjson;
	
    if(http_id_ == -1)
	{
		return;
	}
	
	data = (char *)malloc(HTTP_RSP_MAX_LEN);
	if(!data) return;
	
	memset(data, 0, HTTP_RSP_MAX_LEN);
	http_msg_dequeue(http_id_, data, &size, HTTP_RSP_MAX_LEN);
	if(size == 0)
	{
		free(data);	
		return;	
	}

	std::string tmp(data);
	http_data_ = tmp; // http rsp always is one

	cjson = cJSON_Parse(http_data_.c_str());
	if(cjson == NULL)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] http cjson error: data size=%d \n%s \n%s \n", size, http_data_.c_str(), data);
	    free(data);
		return;
	} 
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP rsp: %s", http_data_.c_str());
	
	cJSON* code    = cJSON_GetObjectItem(cjson, "code");
	if(!code)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP rsp: no code ?");
		cJSON_Delete(cjson);	
		free(data);
		return;
	}
	
	if(0 != code->valueint)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP rsp: code is not 0? code=%d", code->valueint);
		cJSON_Delete(cjson);	
		free(data);
		return;		// TODO: clear current call immediately?
	}

	cJSON* para_data    = cJSON_GetObjectItem(cjson, "data");
	cJSON* branchName   = nullptr;
	if(para_data)
	{
		branchName = cJSON_GetObjectItem(para_data, "branchName");
		if(branchName){
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP get ZLMedia server branchName rsp: %s", branchName->valuestring);
			if(strcmp(branchName->valuestring,"gbcomZlm") == 0 || strcmp(branchName->valuestring,"gbcomZlm2") == 0 || strcmp(branchName->valuestring,"master") == 0)
			{
				peer_server_check_times_ = 0;
				SetPeerServerStatus(1);
				MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]Peer ZLMedia server Ready!");
			}
		}
	}
	
	cJSON* msg     = cJSON_GetObjectItem(cjson, "msg");
	cJSON* message = cJSON_GetObjectItem(cjson, "message");
	if(msg) // rsp msg from rtc media server
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP rsp: code=%d msg=%s login_ok=%d", code->valueint, msg->valuestring, login_ok);
	}
	else if (message) // rsp message from rtc media server
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] HTTP rsp: code=%d message=%s : current login_ok=%d", code->valueint, message->valuestring, login_ok);
	}
	
	cJSON* result = cJSON_GetObjectItem(cjson, "result");
	cJSON* sdp    = cJSON_GetObjectItem(cjson, "sdp");
	cJSON* type   = cJSON_GetObjectItem(cjson, "type");
	
	if(result && !login_ok)
	{
		HttpLoginProcess(result);
	}
	else if(sdp && (0 == strcmp(type->valuestring, "answer")))
	{
		HttpSdpMsgProcess(sdp);
	}
	
	http_data_       = "";
	
	//delete cjson 
	cJSON_Delete(cjson);	
	free(data);
}
////////////////////////////////////////////////////////////////////////////////// 
void PeerConnectionClient::HttpLoginProcess(cJSON* result)
{	
    cJSON* item_map = result->child;

	if(!conn_starting)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] no allow login and discard http msg from server");
		return;		
	}
	
	if(!item_map)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] result no maps?");
		return;		
	}
	
	cJSON* item_i = item_map->child;
	if(!item_i)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] maps no child?");
		return;		
	}
		
    while (item_i != NULL) 
	{
        //MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] Key: %s, Value: %s\n", item_i->string, item_i->valuestring);
		if(0 == strcmp(item_i->string, "token"))
		{
			token_   = item_i->valuestring;
			login_ok = true;
			
			// notify upper http conn is ok
			callback_->OnClientSignedIn();
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] get a new token and start to WebsocketConnect");
			// create websocket conn. 
			WebsocketConnect();			
			//break;
		}
		else if(0 == strcmp(item_i->string, "gbUser"))
		{
			cJSON* item_j = item_i->child;
			while (item_j != NULL) 
			{
				if(0 == strcmp(item_j->string, "id"))
				{
					member_id_ = item_j->valuestring;
					break;
				}
				item_j = item_j->next;
			}
		}
        item_i = item_i->next;
    }	

    if(login_ok)
	{
		// upload local params to server
		HttpCfgReport();
		// report local audio/video capability
		CapabilityReport();
		// upload GB28181 params to server
		//GB28181_CfgReport();
	}
	
	return;
}
////////////////////////////////////////////////////////////////////////////////// 
void PeerConnectionClient::HttpSdpMsgProcess(cJSON* sdp)
{
	int ret;
	
	//{"code":0, "id":"zlm_4431", "sdp":"v=0" }
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] sdp: \n %s ", sdp->valuestring);
	
	ret = OnClientMessageFromPeer(sdp->valuestring);
	
	if(ret > 0)
	{
		WebsocketPushStreamInfo();
		CapabilityReport(); // notify server current capability
		rtc_server_conn_status_update(2); // video transferring
	} else {
		call_event_id_ = "";
	}
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::SdpMessageSend(const std::string& sdp)
{
	std::string  req_str;	
	std::string  http_req_url;	
	std::string  post_data;
	
    if(http_id_ == -1)
	{	
		return;
	}
	
	if(sdp == "")
	{	
		return;
	}
	sdp_msg_ = sdp;
	
	if(peer_server_type_ == 1)
		login_ok = true; // inner server no need to login
	if(!login_ok)
	{	
		return;
	}
	
	StreamClose();
	
	//若设备接WVP服务器，app_id_为member_id_
	//若设备接XV9200服务器，app_id_为inner_server
	//若为大疆无人机模组，则根据MQTT发来的app_id设置
	if(peer_server_type_ != 1)
	{
		app_id_      = member_id_;
	}
	else{
#ifndef MESH_DJI
		app_id_      = "inner_server";
#endif
	}
#ifdef MESH_DJI
	stream_type_ = user_name_;
#else
	stream_type_ = user_name_ + "mesh_t";
#endif
	// fixed : http not use https !!! use http to avoid secure certificate problem.
	play_url_ = "http://" + media_server_address_ + ":" + media_server_port_ + webrtc_api_path + "app=" + app_id_ + "&stream=" + stream_type_ + "&type=" + push_type;

	// space is used to split_http req_url and post_data
	req_str   = play_url_ + " " + sdp_msg_;
	
	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] \n%s \n\n", req_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::StreamClose()
{
	std::string  req_str;	
	std::string  http_req_url;	
	std::string  post_data;
	
    if(http_id_ == -1)
	{	
		return;
	}
	
	if(!login_ok)
	{	
		return;
	}
	
	// Note: secret is used in wvp & mesht device
	req_str = "http://" + media_server_address_ + ":" + media_server_port_ + stream_close_path + "secret=" + webrtc_secret + "&vhost=__defaultVhost__&schema=rtsp&app=" + app_id_ + "&stream=" + stream_type_ + "&force=1";
	
	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] close steam on media server:\n %s ", req_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcMediaSet(const std::string& addr, const std::string& port, const std::string& port_s)
{
	media_server_address_ = addr;
	media_server_port_    = port;
	if(https_using)
	{
		media_server_port_s_  = port_s;
	} else {
		media_server_port_s_  = port;
	}
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcServerSet(const std::string& addr, const std::string& port, const std::string& port_s)
{
	signal_server_address_ = addr + ":" + port;
	// media server is same as signal server in most case
	// If not, using WebrtcMediaSet to setup a diff media serve addr/port
	WebrtcMediaSet(addr, media_server_port_default, media_server_secure_port_default); 
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::Login(const std::string& user, const std::string& passwd, const std::string& video_id)
{	
    if(http_id_ == -1)
	{	
		return;
	}
	if(signal_server_address_ == "")
	{	
		return;
	}
	
	//signal_server_address_ = server;
	user_name_      = user;
	passwd_         = passwd;
	video_id_       = video_id;
	conn_starting   = true;
	
	LoginInternal(0);
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::LoginInternal(int local_retry)
{
	static u_int64_t login_last_time = 0;
	std::string  req_str;	
	std::string  http_req_url;	
	std::string  post_data;
	u_int64_t now = sys_time_get();
	
	if(local_retry)
	{
		now = sys_time_get();
		if((now - login_last_time) < login_retry_interval)
		{
			return; // to avoid send too much login req
		}
		login_last_time = now;
	}
	
	login_ok = false; // reset to false
	
	// '{"username":"st","password":"12345678"}'
	post_data = "{\"username\":\"" + user_name_ + "\",\"password\":\"" + passwd_ + "\"}";
	
	http_req_url = https_url_head + signal_server_address_ + http_login_path;
	// space is used to split_http req_url and post_data
	req_str      = http_req_url + " " + post_data;
	
	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient::Login] (local_retry=%d) %s \n\n", local_retry, req_str.c_str());	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::Logout()
{
	std::string  http_req_url;	
	
	if(!conn_starting)
	{
		return;
	}
	
	conn_starting = false;
	
    if((http_id_ == -1) || !login_ok)
	{	
		return;
	}
	
    // websocket is closed	
	if(websocket_id_ != -1)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] logout: stop current websocket conn: websocket_id_=%d", websocket_id_);
		websocket_session_thread_stop(websocket_id_);
		websocket_id_ = -1;
	}
	
	// webrtc/login/exit/{id}	
	http_req_url = https_url_head + signal_server_address_ + webrtc_logout_path + token_;
	
	http_msg_enqueue(http_id_, http_req_url.c_str());
	
	//callback_->OnClientSignedOut();	
	login_ok = false;
	call_event_id_ = "";
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] logout from webrtc server & close websocket conn");
}
//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionClient::is_connected() const
{
	return   login_ok;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::ServerConnFailed() 
{

}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::RegisterObserver(PeerConnectionClientObserver* callback) 
{
	callback_ = callback;
	http_id_ =  http_thread_start();
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebsocketConnect() 
{
	std::string login_mesh_type = "3";
	std::string ws_url;

	websocket_heartbeat_lost_ = 0;
	if(websocket_id_  != -1)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] [WebsocketConnect] stop prev conn websocket_id_=%d", websocket_id_);
		websocket_session_thread_stop(websocket_id_);
	}
	
	// wss://1.2.3.4:5678/websocket/{token}/{loginType}?{videoId=1234}
	ws_url         = wss_url_head + signal_server_address_ + "/websocket/" + token_ + "/" + login_mesh_type + "?videoId=" + video_id_; // + "&singleMeshT=1";
	websocket_id_  = websocket_session_thread_start(ws_url.c_str());
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] [WebsocketConnect] start new conn  websocket_id_=%d", websocket_id_);

	websocket_heartbeat_time = sys_time_get();
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebsocketMessageHandle(char *data) 
{
	cJSON* cjson;

	std::string tmp(data);
    
	if(websocket_data_ != "")
	{
		if(ws_data_count_ > max_frag_num)
		{
			// peer send error packet ?
			ws_data_count_  = 0;
			websocket_data_ = "";		
		}
		ws_data_count_++;
		websocket_data_ += tmp;
	} else {
		websocket_data_ = tmp;
	}

	cjson = cJSON_Parse(websocket_data_.c_str());		
	if(cjson == NULL)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] websocket cjson error…: %s ", websocket_data_.c_str());
		return;
	} 
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] websocket cjson : %s ", websocket_data_.c_str());

	cJSON *heartbeat   = cJSON_GetObjectItem(cjson, "messageId");
	if(heartbeat)
	{
		if(!heartbeat->valuestring)
		{
			websocket_data_ = "";	
			cJSON_Delete(cjson);	
			return; // invalid msg!
		}
		if(0 == strcmp(heartbeat_msg_id_.c_str(), heartbeat->valuestring))
		{
			//MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] heartBeat ok recv: %s", heartbeat->valuestring);
			websocket_heartbeat_lost_ = 0;
			//rtc_server_conn_status_update(1); 
			websocket_data_ = "";	
			cJSON_Delete(cjson);				
			return;
		}
	}
	
	ws_data_count_  = 0;
			
	cJSON *info   = cJSON_GetObjectItem(cjson, "info");
	cJSON *action = cJSON_GetObjectItem(cjson, "action");
	if(!action || !info)
	{
		cJSON_Delete(cjson);
		websocket_data_ = "";		
		//MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, INFO, "[PeerConnectionClient] action or info not exist?");
		return;
	}
			
	if(action->valuestring == webrtc_video_close)
	{
		// close call 
		WebrtcVideoClose(info);
	}	
	else if(action->valuestring == webrtc_video_call)
	{
		// start call
		WebrtcVideoCall(info);
	}	
	else if(action->valuestring == webrtc_audio_close)
	{
		// close call 
		WebrtcAudioClose(info);
	}	
	else if(action->valuestring == webrtc_audio_call)
	{
		// start call
		WebrtcAudioCall(info);
	}	
	else if(action->valuestring == webrtc_audio_output)
	{
		// start or stop output 
		WebrtcAudioOutputSet(info);
	}		
	else if(action->valuestring == webrtc_cfg_set_req)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] config from server: %s", websocket_data_.c_str());
		WebrtcCfgSet(info);
	}
	else if(action->valuestring == webrtc_gb28181_set_req)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] GB28181 config from server: %s", websocket_data_.c_str());
		Gb28181CfgSet(info);
	}
	else if(action->valuestring == webrtc_ptt_switch)
	{
		WebrtcPttSet(info);
	}	
	else if(action->valuestring == webrtc_cfg_query_req)
	{
		WebrtcCfgQuery(info);
	}
	else if(action->valuestring == webrtc_enterAlarmArea_req)
	{
		WebrtcEnterAlarmArea(info); //Send enter alarm area msg to meshui via IPC msg.
	}
	else if(action->valuestring == webrtc_leaveAlarmArea_req)
	{
		WebrtcLeaveAlarmArea(info); //Send leave alarm area msg to meshui via IPC msg.
	}
	else if(action->valuestring == webrtc_notify_offline)
	{	
		// do nothing
	}
	else if(action->valuestring == webrtc_notify_online)
	{	
	    // do nothing
	}
	else if(action->valuestring == webrtc_fluent_first)
	{
		WebrtcFluentFirstModeSet(info);
	}
	else 
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] action unknown? %s", action->valuestring);
	}
	
	websocket_data_ = "";	
	cJSON_Delete(cjson);	
	return;
}	
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebsocketProcess() 
{
	char *buf;
	int size, ret;
	uint64_t now;
	
	if(websocket_id_ == -1)
	{
		return;
	}
	
	buf = (char *)malloc(WS_MSG_MAX_SIZE);
	if(!buf)
	{
		return;
	}
	
	// recv message from server
	memset(buf, 0, WS_MSG_MAX_SIZE);
	ret = websocket_session_data_dequeue(websocket_id_, buf, &size, WS_MSG_MAX_SIZE);
	if(ret == MESH_RTC_FAIL)
	{
		// websocket conn broken,re-connect it. 
		if(login_ok)
		{
			login_ok = false;
			callback_->OnClientSignedOut();	// notify conductor/conn_ctrl
		}
		if(conn_starting)
		{
			LoginInternal(1); 
		}
		free(buf);
		return;
	}
	
	//MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] [WebsocketProcess]recv:  %s \n", buf);
	if(!conn_starting)
	{
		if(ret > 0)
		{
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] login not allowed and discard all Websocket msg  ");
		}
		free(buf);
		return;		
	}

	if(ret > 0)
	{
		WebsocketMessageHandle(buf);	
	}
	
	// send heartbeat
	now = sys_time_get();
	if((now - websocket_heartbeat_time) < websocket_heartbeat_interval_)
	{
		free(buf);
		return;
	}
	websocket_heartbeat_time = now;
	websocket_heartbeat_lost_++;
	if(websocket_heartbeat_lost_ > heartbeat_lost_max)
	{
		websocket_heartbeat_lost_ = 0; // next check cycle
		rtc_server_conn_status_update(0); 
		ptt_capability_set(true, true);
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]websocket_heartbeat_lost_ & disconn from server and re-open ptt ");
		
		if(call_event_id_ != "")
		{
			callback_->OnClientHangup(); 
			call_event_id_ = ""; // current push stream cleaned
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] close connection and clean push stream info ");		
		}
		
		// re-connect to rtc server. 20240708
		login_ok = false;
		callback_->OnClientSignedOut();	// notify conductor/conn_ctrl
		if(conn_starting)
		{
			LoginInternal(1); 		
		}
		free(buf);
		return;		
	}
	/*
		{
			"action": "heartBeat",
			messageid: “随机id标识这个信息”,
		}
	*/
	heartbeat_msg_id_ = "heartbeat_" + std::to_string(websocket_heartbeat_time);
	sprintf(buf, "{\"action\":\"heartBeat\", messageid:\"%s\"}", heartbeat_msg_id_.c_str());
	ret = websocket_session_data_enqueue(websocket_id_, buf, strlen(buf));	
	if(ret == MESH_RTC_FAIL)
	{
		// websocket conn broken,re-connect it. 
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]websocket send error! re-connect to server  ");
		login_ok = false;
		callback_->OnClientSignedOut();	// notify conductor/conn_ctrl
		if(conn_starting)
		{
			LoginInternal(1); 
		}
	}	
	free(buf);
}
//////////////////////////////////////////////////////////////////////////////////
std::string  PeerConnectionClient::StreamInfo()
{
/*
{
    "playUrl":"http://10.30.2.8:6080/index/api/webrtc?app=a8aa6908545e40379265dd85089deb10&stream=lw5stream&type=play",
    "playUrls":"https://10.30.2.8:10443/index/api/webrtc?app=a8aa6908545e40379265dd85089deb10&stream=lw5stream&type=play",
    "zlmHost":"https://10.30.2.8:10443",
    "app":"lw5",
    "stream":"a8aa6908545e40379265dd85089deb10",
    "type":"play",
    "simulcast":false,
    "useCamera":true,
    "audioEnable":false,
    "videoEnable":false,
    "recvOnly":true,
    "usedatachannel":false,
    "createrId":"a8aa6908545e40379265dd85089deb10",
    "createTime":"2023-11-21T01:42:13.336+00:00"
}
*/
	std::string info, addr1, addr2, media_addr;

	if(app_id_ == "")
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] app_id_ is empty? Now set it.");
		app_id_ = member_id_; // default app_id
	}

	if(stream_type_ == "")
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] stream_type_ is empty? Now set it.");
		stream_type_ = user_name_ + "mesh_t"; // default stream_type
	}
	
	addr1 = media_server_address_ + ":" + media_server_port_;
	std::string playUrl  = "\"playUrl\":\""  + http_url_head  + addr1 + webrtc_api_path + "app=" + app_id_ + "&stream=" + stream_type_  + "&type=" + play_type + "\",";
	
	addr2 = media_server_address_ + ":" + media_server_secure_port_default;	// use fixed secure port
	// always use https !!!
	std::string playUrls = "\"playUrls\":\"https://" + addr2 + webrtc_api_path + "app=" + app_id_ + "&stream=" + stream_type_  + "&type=" + play_type + "\","; 

	media_addr = media_server_address_ + ":" + media_server_secure_port_default; // use fixed secure port
	std::string zlmHost  = "\"zlmHost\":\"https://" + media_addr + "\","; // zlm host always use https !!!!
	
	std::string app      = "\"app\":\"" + app_id_  + "\",";
	
	std::string stream_id = "\"stream\":\"" + stream_type_  + "\", ";
	std::string type      = "\"type\":\"" + play_type + "\",";
	std::string simulcast = "\"simulcast\":false, ";
	std::string useCamera = "\"useCamera\":false, ";
	std::string audioEnable = "\"audioEnable\":true, ";
	std::string videoEnable = "\"videoEnable\":false, ";
	if(video_enable_) {
		videoEnable = "\"videoEnable\":true, ";
		useCamera = "\"useCamera\":true, ";
	}
	std::string recvOnly    = "\"recvOnly\":true, ";
	
	std::string usedatachannel = "\"usedatachannel\":false, ";
	create_id_++;
	std::string createrId      = "\"createrId\":\"" + std::to_string(create_id_) + "\", ";
	std::string createTime     = "\"createTime\":\"2024-01-09T10:15:13\" ";	// only for test & debug. Not used in release version
	
	info = "{" + playUrl + playUrls + zlmHost  + app + stream_id + type + simulcast + useCamera + audioEnable + videoEnable + recvOnly + usedatachannel + createrId + createTime + "}";
	
	return info;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebsocketPushStreamInfo()
{
	std::string now = std::to_string(sys_time_get());
	std::string push_req;
	std::string action;
	std::string token;
	std::string messageid;
	std::string params;
	std::string eventId;
	std::string streaminfo = StreamInfo();
	char buf[2048];
	/*
	{
    "action": "pushStream",
	"token": "sdfasdfsd",
	messageid: “随机id标识这个信息”,
	param: {
		“eventId”:”点播命令发来的标志”
		"streaminfos": [
			{ //streaminfo 推流地址 名称 视频 音频 等  待定义
			}
		],
	}	
	*/
	action    = "\"action\": \"pushStream\",";
	token     = "\"token\": \"" + token_ + "\", ";
	messageid = "\"messageid\": \"" + now + "\","; // use sys time as random number
	//messageid = "\"messageid\": \"" + std::string("12345678") + "\","; // use sys time as random number

	eventId   = "\"eventId\": \"" + call_event_id_ + "\" ";
	params    = "\"param\": { " + eventId + ", \"streamInfos\": [" + streaminfo + " ] } ";

	push_req = "{" + action + token + messageid + params + "}";
	
	sprintf(buf, "{\"action\":\"heartBeat\", messageid:\"%lu\"}", websocket_heartbeat_time);
	
	websocket_session_data_enqueue(websocket_id_, push_req.c_str(), push_req.size());	
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] %s", push_req.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
int PeerConnectionClient::OnClientMessageFromPeer(const std::string& message)
{
	return callback_->OnClientMessageFromPeer(message);
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcVideoCall(cJSON *info) 
{
	cJSON*       item_i = info->child;
	std::string  call_id;

	while (item_i != NULL) 
	{
		if(0 == strcmp(item_i->string, "eventId"))
		{
			call_id = item_i->valuestring;
		}
        item_i = item_i->next;
	}

	if(call_event_id_ != "")		
	{
		// If push stream exist, update call_event_id_
		call_event_id_ = call_id;
		WebsocketPushStreamInfo(); 
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] push stream exist and reuse it! call_event_id_=%s", call_event_id_.c_str());		
		return;
	} 		
	
	call_event_id_ = call_id;
	
	// we use audio & video default
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] streamInfo not exist? audio & video all on!!! call_event_id_=%s", call_event_id_.c_str());		
	callback_->OnClientCalled(audio_enable_, video_enable_);
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] audio_enable=%d video_enable=%d \n", audio_enable_, video_enable_);		
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcStreamClean()
{
	call_event_id_ = "";
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] WebrtcStreamClean");		
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcVideoClose(cJSON *info) 
{
		std::string   close_event_id = "";
		cJSON*        item_i = info->child;
		
		while (item_i != NULL) 
		{
			if(0 == strcmp(item_i->string, "eventId"))
			{
				close_event_id = item_i->valuestring;
			}
			item_i = item_i->next;
		}	
		
		if(call_event_id_ == "")
		{
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] no video stream for close... close_event_id=%s", close_event_id.c_str());	
			return;
		}

		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] action closePlayVideoMsg... close_event_id=%s", close_event_id.c_str());			
		callback_->OnClientHangup(); 
		call_event_id_ = ""; // current push stream cleaned
}
/*
回复
{
    “code”: 0
	“messageId”: “这个信息的随机id”,
}
*/
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcAudioCall(cJSON *info) 
{
/*
	{
    “action”: “readyForCall”,
	“token”: “sdfasdfsd”,
	“messageId”: “随机id标识这个信息”,
	“param”: {
		“eventId”:”喊话命令发来的标志”
		“code”：0 	// 0 成功    其他不成功  待定义
 		“message” : “同意”
	}
}
*/
	cJSON*       item_i = info->child;
	cJSON*       item_stream = NULL;
	std::string  event_id;

	// check eventId and stream info
	while (item_i != NULL) 
	{
		if(0 == strcmp(item_i->string, "eventId"))
		{
			event_id = item_i->valuestring;
		}
		else if(0 == strcmp(item_i->string, "streamInfos"))
		{
			// streamInfos
			item_stream = item_i;
		}
        item_i = item_i->next;
	}
	if((event_id == "") || !item_stream)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  WebrtcAudioCall is invalid: event_id=%s ", event_id.c_str());
		return;
	}
	
	// get 	playUrl	
	std::string  playUrl;
	item_i = item_stream->child;
	if(item_i == NULL) 
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  WebrtcAudioCall is invalid: stream info ");
		return;
	}
	
	item_i = item_i->child;
	while (item_i != NULL) 
	{
		if(!item_i->string)
		{
			item_i = item_i->next;
			continue;
		}
		
		if(0 == strcmp(item_i->string, "playUrl"))
		{
			playUrl = item_i->valuestring;
			break;
		}
		else if(0 == strcmp(item_i->string, "playUrls"))
		{
			//playUrl = item_i;
		}
        item_i = item_i->next;
	}	
	if(playUrl == "")
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  WebrtcAudioCall is invalid: playUrl does not exist ");
		return;
	}

	// space is used to split_http req_url and post_data 
	// send play req with sdp to media server
	std::string  req_str   = playUrl + " " + sdp_msg_;	
	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] SdpMessageSend for play media \n%s ", req_str.c_str());
	
	// this rsp should send late. here is not correct time.
	std::string  call_rsp;
	std::string  call_action  = "\"action\": \"readyForCall\", \n";
	std::string  call_token   = "\"token\": \"readyForCall\", \n";
	std::string  call_msg_id  = "\"messageId\": \"1234567890\", \n";

	std::string  param_event_id =  "\"eventId\": \"" + event_id + "\", \n";;
	std::string  param_code   = "\"code\": 0, \n";
	std::string  param_msg    = "\"message\": \"ok\" \n";;
	std::string  call_param   = "\"param\": { \n" + param_event_id + param_code + param_msg + " }";;
	
	call_rsp = "{ \n" + call_action + call_token + call_msg_id + call_param + "  }";

	websocket_session_data_enqueue(websocket_id_, call_rsp.c_str(), call_rsp.size());	
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] WebrtcAudioCall playUrl:\n%s \n%s", playUrl.c_str(), call_rsp.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcAudioClose(cJSON *info) 
{
	// it is not used now
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcAudioOutputSet(cJSON *info) 
{
	cJSON*       item_i = info->child;
	std::string  event_id;

	// check eventId and stream info
	while (item_i != NULL) 
	{
		if(0 == strcmp(item_i->string, "eventId"))
		{
			event_id = item_i->valuestring;
		}
        item_i = item_i->next;
	}
	if(event_id == "")
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  WebrtcAudioCall is invalid: event_id=%s ", event_id.c_str());
		return;
	}
	
	//TODO
	audio_output_disable();
	audio_output_enable();
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcCfgQuery(cJSON *info) 
{
	int fps_flag, bps_flag, res_flag, osd_flag;
	char event_id[256];
	
	int ret = CfgQueryDecode(info, &fps_flag, &bps_flag, &res_flag, &osd_flag, event_id);
	if(ret <= 0)	
	{
		return;
	}
	cfg_event_id_ =  std::string(event_id);
	
	WebrtcCfgQueryRsp(cfg_event_id_, fps_flag, bps_flag, res_flag, osd_flag);
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcCfgQueryRsp(std::string msg_id, int fps_flag, int bps_flag, int res_flag, int osd_flag) 
{
	std::string query_rsp;

	query_rsp  = CfgQueryRspEncode(msg_id, fps_flag, bps_flag, res_flag, osd_flag, &av_params) ;
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] query_rsp:\n%s", query_rsp.c_str());
	
	websocket_session_data_enqueue(websocket_id_, query_rsp.c_str(), query_rsp.size());		
}
///////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////预警圈相关需求接口//////////////////////////////
void PeerConnectionClient::WebrtcEnterAlarmArea(cJSON *info)
{
    std::string imgPath = "";
    char enterAlarmAreaMsg[256] = {0};
    // 直接从 info 中获取 imgPath
    cJSON* img = cJSON_GetObjectItem(info, "imgPath");
    if(img && img->valuestring)
    {
        imgPath = img->valuestring;
        MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] imgPath: %s", imgPath.c_str());
		if(imgPath == "")
		{
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] imgPath is empty.");
			return;
		}
        sprintf(enterAlarmAreaMsg,"enterAlarmArea %s",imgPath.c_str());
		ipc_msg_send(MESH_UI_ALARM_AREA_MODULE_NAME, enterAlarmAreaMsg, sizeof(enterAlarmAreaMsg));
	}
    else
    {
        MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_ERR, "[PeerConnectionClient] imgPath not found or is null");
    }
}
void PeerConnectionClient::WebrtcLeaveAlarmArea(cJSON *info)
{
	char leaveAlarmAreaMsg[] = "leaveAlarmArea";
	ipc_msg_send(MESH_UI_ALARM_AREA_MODULE_NAME, leaveAlarmAreaMsg, sizeof(leaveAlarmAreaMsg));
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcCfgSet(cJSON *info) 
{
	av_osd_cfg_t   *osd = &av_params.osd_cfg;
	int ret;
	char eventId[128];
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " Webrtc Cfg Set: close conn to use the latest video config data... ");	
	callback_->OnClientHangup();
	sleep_ms(100); 
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  Webrtc Cfg Set msg ....");		
	
	// if config params is ok , update local av_params
	ret = CfgSetDecode(info, &av_params, eventId); 
	if(ret <= 0)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  Webrtc Cfg Set deocde error!");		
		return;
	}
	cfg_event_id_ = std::string(eventId);
	
	// set params to av module. 
	video_cfg_set((char *)&av_params, sizeof(av_params));
	
	// save cfg to video encoder
	video_osd_cfg_update(osd);

	// store params to config file. 
	av_video_cfg_set(&av_params);
	
	av_video_cfg_dump(&av_params);
	
	WebrtcCfgSetRsp(cfg_event_id_, ret);
		
	return;	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcCfgSetRsp(std::string event_id, int result) 
{
	std::string cfg_msg;
	
	cfg_msg = CfgSetRspEncode(event_id, result, &av_params);

	std::string rsp = "{  \"action\": \"deviceConfigReplyMsg\", \"token\": \"" + token_ + "\", \"messageId\": \"_random_\", " + cfg_msg + " }";

	websocket_session_data_enqueue(websocket_id_, rsp.c_str(), rsp.size());	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::TriggerCfgReport()
{
	cfg_report_flag_ = 1;	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::Gb28181CfgSet(cJSON *info) 
{
	int ret;
	char eventId[128];

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  Gb28181 Cfg Set msg ....");		
	
	ret = GB28181_CfgSetDecode(info, &gb_28181_cfg, eventId); 
	if(ret <= 0)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  Gb28181 Config Decode error!");		
		return;
	}
	cfg_event_id_ = std::string(eventId);
	
	//保存国标28181配置到本地prop
	gb_28181_cfg_set(&gb_28181_cfg);

	if (gb_28181_cfg.gb28181_enable == 1)
	{
		system("start term_gb_client");
		sleep_ms(500);
		system("start 28181_client");
	}
	else
	{
		system("start term_gb_client");
	}

	Gb28181CfgSetRsp(cfg_event_id_, ret);
		
	return;	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::Gb28181CfgSetRsp(std::string event_id, int result) 
{
	std::string cfg_msg;
	
	cfg_msg = Gb28181_CfgSetRspEncode(event_id, result, &gb_28181_cfg);

	std::string rsp = "{  \"action\": \"gb28181ConfigReplyMsg\", \"token\": \"" + token_ + "\", \"messageId\": \"_random_\", " + cfg_msg + " }";

	websocket_session_data_enqueue(websocket_id_, rsp.c_str(), rsp.size());	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::TriggerGb28181Report()
{
	gb28181_report_flag_ = 1;	
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::HttpCfgReport()
{		
	std::string params_str = CfgReportEncode(&av_params);;

	std::string http_req_url = https_url_head + signal_server_address_ + http_cfg_chg_path + member_id_;
	
	// space is used to split_http req_url and post_data
	std::string req_str      = http_req_url + " " + params_str;

	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] cfg report:\n%s", req_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::GB28181_CfgReport()
{		
	std::string params_str = GB28181_CfgEncode(&gb_28181_cfg);

	std::string http_req_url = https_url_head + signal_server_address_ + http_28181_cfg_path + member_id_;
	
	// space is used to split_http req_url and post_data
	std::string req_str      = http_req_url + " " + params_str;

	http_msg_enqueue(http_id_, req_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] gb28181 cfg report:\n%s", req_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::HttpCfgUpload() // not used. this interface is for web client
{		
	std::string params_str = CfgEncode(&av_params);;
	cJSON* cjson;
	
	std::string http_req_url = https_url_head + signal_server_address_ + http_cfg_set_path + member_id_;
	
	// space is used to split_http req_url and post_data
	std::string req_str      = http_req_url + " " + params_str;

	http_msg_enqueue(http_id_, req_str.c_str());
	
	cjson = cJSON_Parse(params_str.c_str());
	if(cjson == NULL)
	{
		MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] cfg upload error:\n%s", params_str.c_str());
	}

	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] cfg upload:\n%s", req_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::CapabilityReport()
{		
	std::string params_str = CapabilityEncode(member_id_, audio_enable_, video_enable_);;

	std::string http_req_url = https_url_head + signal_server_address_ + capability_report_path;
	
	// Note: http req_url and get !!!
	std::string report_str   = http_req_url + params_str;
	//std::string report_str   = http_req_url  + " " +  params_str;

	http_msg_enqueue(http_id_, report_str.c_str());
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] capability report:\n%s", report_str.c_str());
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcPttSet(cJSON *info) 
{
	std::string  pttSwitch;
	std::string  call_id;
	int  ptt_set_value = -1;
    cJSON* item_i = info->child;
	if(!item_i)
	{
		return;		
	}
	// check eventId and stream info
	while (item_i != NULL) 
	{
		if(strcmp(item_i->string, "pttSwitch") == 0)
		{
			if(item_i->valueint)
			{
				ptt_capability_set(true, true);
				ptt_set_value = 1;
			} else {
				ptt_capability_set(false, false);
				ptt_set_value = 0;
			}
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] ptt send/recv set to %d", item_i->valueint);
			item_i = item_i->next;
			continue;
		}
		if(strcmp(item_i->string, "eventId") == 0)
		{
			call_id = item_i->valuestring;
			item_i = item_i->next;
			continue;
		}
		item_i = item_i->next;
	}
	WebrtcPttRsp(call_id, ptt_set_value);
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionClient::WebrtcPttRsp(std::string event_id, int ptt_set_value) 
{
	std::string cfg_msg;
	std::string ptt_set_str;

	if(ptt_set_value == 1)
	{
		ptt_set_str = "true";
	} else {
		ptt_set_str = "false";
	}

	std::string rsp = "{  \"action\": \"pttSwitchReplyMsg\", \"token\": \"" + token_ + "\", \"messageId\": \"_random_\", \"param\": { \"eventId\": \"" + event_id + "\",\"code\":" + std::to_string(0) + ",\"info\":{\"pttSwitch\":" + ptt_set_str + "}}}";

	websocket_session_data_enqueue(websocket_id_, rsp.c_str(), rsp.size());
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] rsp sent :ptt send/recv set to %d", ptt_set_value);
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] \n%s", rsp.c_str());
}
//////////////////////////////////////////////////////////////////////////////////

void PeerConnectionClient::WebrtcFluentFirstModeSet(cJSON *info){
	std::string  call_id;
	int  fluentfirstmode_set_value = -1;
    cJSON* item_i = info->child;
	if(!item_i)
	{
		return;		
	}
	// check eventId and stream info
	while (item_i != NULL) 
	{
		if(strcmp(item_i->string, "fluentFirstMode") == 0)
		{
			if(item_i->valueint)
			{
				cfg_rcmode_set(1);
				fluentfirstmode_set_value = 1;
			} else {
				cfg_rcmode_set(0);
				fluentfirstmode_set_value = 0;
			}
			MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient]  FluentFirstMode set to %d", item_i->valueint);
			item_i = item_i->next;
			continue;
		}
		if(strcmp(item_i->string, "eventId") == 0)
		{
			call_id = item_i->valuestring;
			item_i = item_i->next;
			continue;
		}
		item_i = item_i->next;
	}
	WebrtcFluentFirstModeRsp(call_id, fluentfirstmode_set_value);	
}

void PeerConnectionClient::WebrtcFluentFirstModeRsp(std::string event_id, int fluentfirstmode) 
{
	std::string cfg_msg;
	std::string fluentfirstmode_str;

	if(fluentfirstmode == 1)
	{
		fluentfirstmode_str = "true";
	} else {
		fluentfirstmode_str = "false";
	}

	std::string rsp = "{  \"action\": \"fluentFirstModeReplyMsg\", \"token\": \"" + token_ + "\", \"messageId\": \"_random_\", \"param\": { \"eventId\": \"" + event_id + "\",\"code\":" + std::to_string(0) + ",\"info\":{\"fluentFirstMode\":" + fluentfirstmode_str + "}}}";

	websocket_session_data_enqueue(websocket_id_, rsp.c_str(), rsp.size());
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] rsp sent :FluentFirstMode set to %d", fluentfirstmode);
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "[PeerConnectionClient] \n%s", rsp.c_str());
}