/*

 */

#ifndef __PC_CLIENT_CONDUCTOR_H_
#define __PC_CLIENT_CONDUCTOR_H_

#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/media_stream_interface.h"
#include "api/peer_connection_interface.h"

#ifdef MAIN_WND_USING
#include "main_wnd.h"
#else
#include "conn_ctrl.h"
#endif

#include "peer_connection_mesh.h"
#include "peer_connection_client.h"

///////////////////////////////////////////////////////////////////////////
namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

void AudioSamplesChange(int samples);
///////////////////////////////////////////////////////////////////////////
class Conductor : public webrtc::PeerConnectionObserver,
                  public webrtc::CreateSessionDescriptionObserver,
                  public PeerConnectionMeshObserver,
				  public PeerConnectionClientObserver,
                  public MainWndCallback {
 public:

  Conductor(PeerConnectionMesh* client_m, PeerConnectionClient* client_c, MainWindow* main_wnd, const char *peer_ip, const char *local_ip, int samples);

  bool connection_active() const;
  void Close() override;

  // only-audio call without video
  void video_enable(bool enable);
  void local_video_use(bool local_src);
  void OnClientHangup() override;
  void SetAppName(const std::string& app_name);
  void SetStreamID(const std::string& user);
  void SetServerType(int server_type);
  void GetInnerServerStatus();
  int  GetServerStatusFlag();
  void SetServerStatusFlag(int server_status_flag);
  
 protected:
  ~Conductor();
  bool InitializePeerConnection(bool wh_limited);

  bool CreatePeerConnection(bool dtls, bool wh_limited);
  void DeletePeerConnection();
  void EnsureStreamingUI();
  void AddTracks();

  //
  // PeerConnectionObserver implementation.
  //

  void OnSignalingChange(
      webrtc::PeerConnectionInterface::SignalingState new_state) override {}

  void OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state) override;

  void OnAddTrack(
      webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
      const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>&
          streams) override;
  void OnRemoveTrack(
      webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override;

  void OnDataChannel(
      webrtc::scoped_refptr<webrtc::DataChannelInterface> channel) override {}
  void OnRenegotiationNeeded() override {}
  void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) override;
  void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) override {}
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;
  void OnIceConnectionReceivingChange(bool receiving) override {}

  //
  //PeerConnectionMeshObserver implementation.
  //
  void OnMeshSignedIn(int id, const std::string& name, bool connected)  override;

  void OnMeshDisconnected(int reason) override;

  void OnMeshPeerConnected(int id, const std::string& name) override;

  void OnMeshPeerDisconnected(int id) override;

  void OnMeshMessageFromPeer(int peer_id, const std::string& message) override;

  void OnMeshMessageSent(int err) override;

  void OnMeshServerConnectionFailure(int reason) override;

  //
  //PeerConnectionClientObserver implementation.
  //
  void OnClientSignedIn() override;  // Called when we're logged on.

  void OnClientSignedOut() override;

  void OnClientPeerConnected() override;

  void OnClientPeerDisconnected() override;

  int  OnClientMessageFromPeer(const std::string& message) override;

  void OnClientCalled(bool audio_enable, bool video_enable) override;
  
  //void OnClientHangup() override;
  
  void OnClientMessageSent(int err) override;

  void OnClientServerConnectionFailure() override;
  
  void WebrtcServerSet(const std::string& addr, const std::string& port, const std::string& port_s);
  void WebrtcMediaSet(const std::string& addr, const std::string& port, const std::string& port_s);
  
  void StartWebrtcLogin(const std::string& user, const std::string& passwd, const std::string& video_id);
  void SendWebrtcSdp(const std::string& sdp);	
  void StartWebrtcLogout();
  //
  // MainWndCallback implementation.
  //

  void StartLogin(const std::string& server, int port, const char *name) override;

  void DisconnectFromServer() override;

  void ConnectToPeer(int peer_id) override;
  
  void ConnectToRtcServer() override;

  void DisconnectFromCurrentPeer(int status) override;

  void UIThreadCallback(int msg_id, void* data) override;

  std::string sdpMsgModify(std::string& sdp);

  // CreateSessionDescriptionObserver implementation.
  void OnSuccess(webrtc::SessionDescriptionInterface* desc) override;
  void OnFailure(webrtc::RTCError error) override;

 protected:
  // Send a message to the remote peer.
  void SendMessage(const std::string& json_object);
  void PeerIdSet(int peer_id, int loc);
  
  int  pcm_sampling_rate_;
  int  peer_id_;

  bool video_call_;
  bool video_local_src_;

  int video_server_type_;
  
  webrtc::scoped_refptr<webrtc::PeerConnectionInterface>  peer_connection_;
  webrtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>  peer_connection_factory_;
  PeerConnectionMesh    *client_m_;
  PeerConnectionClient  *client_c_;
  
  MainWindow* main_wnd_;
  
  std::deque<std::string*> pending_messages_; // save all webrtc msg
  
  std::string server_;
};

#endif  // __PC_CLIENT_CONDUCTOR_H_
