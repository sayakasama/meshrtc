/*

*/

#ifndef __PC_PEER_CONNECTION_CLIENT_H_
#define __PC_PEER_CONNECTION_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include "cjson.h"

struct PeerConnectionClientObserver {
  virtual void OnClientSignedIn() = 0;  // Called when we're logged on.
  virtual void OnClientSignedOut() = 0;
  virtual void OnClientPeerConnected() = 0;
  virtual void OnClientPeerDisconnected() = 0;
  virtual int  OnClientMessageFromPeer(const std::string& message) = 0;
  virtual void OnClientCalled(bool audio_enable, bool video_enable) = 0;
  virtual void OnClientHangup() = 0;
  virtual void OnClientMessageSent(int err) = 0;
  virtual void OnClientServerConnectionFailure() = 0;

 protected:
  virtual ~PeerConnectionClientObserver() {}
};

class PeerConnectionClient {
 public:

  PeerConnectionClient(bool only_audio);
  ~PeerConnectionClient();

  void WebrtcMediaSet(const std::string& addr, const std::string& port, const std::string& port_s);
  void WebrtcServerSet(const std::string& addr, const std::string& port, const std::string& port_s);

  void Login(const std::string& user, const std::string& passwd, const std::string& video_id);
  void Logout();
  int  Start();
  void SdpMessageSend(const std::string& sdp);

  void SetAppName(const std::string& app_name);
  void SetStreamID(const std::string& user);
  void GetInnerServerStatus();

  void StreamClose();
  
  void Process();

  bool is_connected() const;
  
  void ServerConnFailed();

  void RegisterObserver(PeerConnectionClientObserver* callback);

  void TriggerCfgReport();

  void TriggerGb28181Report();
  
  void WebrtcStreamClean();

  void SetPeerServerType(int server_type);
  void SetPeerServerStatus(int server_status);

  int peer_server_check_times_;
  int peer_server_type_; // 0: WVP, 1: XV9100
  int peer_server_status_; // 0: not ready, 1: ready 2: pushing stream
  
protected:
  void HttpProcess();
  void LoginInternal(int local_retry);
  
  void WebsocketConnect();
  void WebsocketProcess();
  int  OnClientMessageFromPeer(const std::string& message);

  void WebsocketMessageHandle(char *data);
  void WebsocketPushStreamInfo();
  std::string StreamInfo();
  
  void HttpLoginProcess(cJSON* cjson);
  void HttpSdpMsgProcess(cJSON* cjson);
  
  // webrtc msg proc
  void WebrtcVideoClose(cJSON *info);
  void WebrtcVideoCall(cJSON *info);
  void WebrtcAudioClose(cJSON *info);
  void WebrtcAudioCall(cJSON *info);
  void WebrtcAudioOutputSet(cJSON *info);

  void WebrtcFluentFirstModeSet(cJSON *info);
  void WebrtcFluentFirstModeRsp(std::string event_id, int fluentfirstmode);

  void WebrtcEnterAlarmArea(cJSON *info);
  void WebrtcLeaveAlarmArea(cJSON *info);
    
  void WebrtcCfgQuery(cJSON *info); 
  void WebrtcCfgQueryRsp(std::string msg_id, int fps, int bps, int res, int osd); 

  void WebrtcCfgSet(cJSON *info);
  void WebrtcCfgSetRsp(std::string event_id, int result);

  void Gb28181CfgSet(cJSON *info);
  void Gb28181CfgSetRsp(std::string event_id, int result);
  
  void WebrtcPttSet(cJSON *info);
  void WebrtcPttRsp(std::string event_id, int ptt_set_value);
  
  void HttpCfgReport();
  void HttpCfgUpload(); //not used
  
  void GB28181_CfgReport();
  
  void CapabilityReport();
  
  //////////////////////////////////////	
  PeerConnectionClientObserver* callback_;
  
  int           cfg_report_flag_;
  int           gb28181_report_flag_;

  int           create_id_;	
  std::string   media_server_address_;
  std::string   media_server_port_; 
  std::string   media_server_port_s_; 
  std::string   app_id_;
  std::string   stream_type_;
  
  std::string   signal_server_address_;
  std::string  	user_name_;
  std::string 	passwd_   ;
  std::string 	video_id_;
  
  std::string   sdp_msg_;	
  std::string   token_;
  std::string   member_id_;
  
  std::string   play_url_;
  
  //bool push_stream_info_;
  bool          audio_enable_;
  bool          video_enable_;
  std::string   call_event_id_;

  std::string   cfg_event_id_;
  
  int           http_id_;
  int           websocket_id_;
  uint64_t      websocket_heartbeat_time;
  unsigned int  websocket_heartbeat_interval_; // ms
  unsigned int  websocket_heartbeat_lost_;
  std::string   heartbeat_msg_id_;
  
  std::string   websocket_data_;
  unsigned int  ws_data_count_;
  
  std::string   http_data_;
  
  bool login_ok;
  bool conn_starting;
};

#endif  // __PC_PEER_CONNECTION_CLIENT_H_
