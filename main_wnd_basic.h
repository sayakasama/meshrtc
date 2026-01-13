/*
 */

#ifndef __PC_CLIENT_MAIN_WND_H_
#define __PC_CLIENT_MAIN_WND_H_

#include <map>
#include <memory>
#include <string>

#include "api/media_stream_interface.h"
#include "api/video/video_frame.h"
#include "peer_connection_mesh.h"
#include "media/base/media_channel.h"
#include "media/base/video_common.h"

class MainWndCallback {
 public:

  virtual void WebrtcServerSet(const std::string& addr, const std::string& port, const std::string& port_s) = 0;
  virtual void WebrtcMediaSet(const std::string& addr, const std::string& port, const std::string& port_s) = 0;
  
  virtual void StartWebrtcLogin(const std::string& user, const std::string& passwd, const std::string& video_id) = 0;
  virtual void SendWebrtcSdp(const std::string& sdp) = 0;
  virtual void StartWebrtcLogout() = 0;
  
  virtual void StartLogin(const std::string& server, int port, const char *name) = 0;
  virtual void DisconnectFromServer() = 0;
  
  virtual void ConnectToPeer(int peer_id) = 0;
  virtual void DisconnectFromCurrentPeer(int status) = 0;
  
  virtual void UIThreadCallback(int msg_id, void* data) = 0;
  virtual void Close() = 0;

  virtual void ConnectToRtcServer() = 0;
  
 protected:
  virtual ~MainWndCallback() {}
};

// Pure virtual interface for the main window.
class MainWindow {
 public:
  virtual ~MainWindow() {}

  enum UI {
    CONNECT_TO_SERVER,
    LIST_PEERS,
    STREAMING,
  };

  // following function is not implemented by conn_ctrl
  virtual void SwitchToConnectUI() {};
  virtual UI current_ui() { return STREAMING;};  
  virtual void SwitchToPeerList(const Peers& peers) {};
  virtual void SwitchToStreamingUI() {};

  virtual void StartLocalRenderer(webrtc::VideoTrackInterface* local_video) {};
  virtual void StopLocalRenderer() {};

  // following must be implemented
  virtual void RegisterObserver(MainWndCallback* callback) = 0;

  virtual bool IsWindow() = 0;

  virtual void StartRemoteRenderer(webrtc::VideoTrackInterface* remote_video) = 0;
  virtual void StopRemoteRenderer() = 0;

  virtual void ConnectedPeerResult(bool result) = 0;
  virtual void QueueUIThreadCallback(int msg_id, void* data) = 0;
  
  virtual void OnMeshSignFailed()  = 0;  
  virtual void OnMeshSignedIn(int id, const std::string& name, bool connected)  = 0;
  virtual void OnMeshDisconnected(int reason)  = 0;
  virtual void OnMeshPeerConnected(int id, const std::string& name) = 0;
  virtual void OnMeshPeerDisconnected() = 0;
};

#endif  // __PC_CLIENT_MAIN_WND_H_
