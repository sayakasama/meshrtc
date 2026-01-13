/*

 */

#ifndef __PC_PEER_CONNECTION_MESH_H_
#define __PC_PEER_CONNECTION_MESH_H_

#include <map>
#include <memory>
#include <string>

#include "rtc_base/net_helpers.h"
#include "rtc_base/physical_socket_server.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/async_socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/third_party/sigslot/sigslot.h"

typedef std::map<int, std::string> Peers;

struct PeerConnectionMeshObserver {
  virtual void OnMeshSignedIn(int id, const std::string& name, bool connected) = 0;  // Called when we're logged on.
  virtual void OnMeshDisconnected(int reason) = 0;
  virtual void OnMeshPeerConnected(int id, const std::string& name) = 0;
  virtual void OnMeshPeerDisconnected(int peer_id) = 0;
  virtual void OnMeshMessageFromPeer(int peer_id, const std::string& message) = 0;
  virtual void OnMeshMessageSent(int err) = 0;
  virtual void OnMeshServerConnectionFailure(int reason) = 0;

 protected:
  virtual ~PeerConnectionMeshObserver() {}
};

class PeerConnectionMesh : public sigslot::has_slots<>,
                             public webrtc::MessageHandler {
 public:
  enum State {
    NOT_CONNECTED,
    RESOLVING,
    SIGNING_IN,
    CONNECTED,
    SIGNING_OUT_WAITING,
    SIGNING_OUT,
  };

  PeerConnectionMesh();
  ~PeerConnectionMesh();

  int id() const;
  bool is_logined() const;
  bool is_connected() const;
  const Peers& peers() const;
  
  void  ServerConnFailed();

  void RegisterObserver(PeerConnectionMeshObserver* callback);

  void Stop();	
  void Start();	
  
  void Connect(const std::string& server,
               int port,
               const std::string& client_name);

  bool SendToPeer(int peer_id, const std::string& message);
  bool SendHangUp(int peer_id);
  bool IsSendingMessage();

  bool SignOut();
  void Close(int reason);
  
  // implements the MessageHandler interface
  void OnMessage(webrtc::Message* msg);

 protected:
  void DoConnect(int reason);
  
  void InitSocketSignals();
  bool ConnectControlSocket();
  void OnConnect(webrtc::AsyncSocket* socket);
  void OnHangingGetConnect(webrtc::AsyncSocket* socket);
  void OnMeshMessageFromPeer(int peer_id, const std::string& message);

  // Quick and dirty support for parsing HTTP header values.
  bool GetHeaderValue(const std::string& data,
                      size_t eoh,
                      const char* header_pattern,
                      size_t* value);

  bool GetHeaderValue(const std::string& data,
                      size_t eoh,
                      const char* header_pattern,
                      std::string* value);

  // Returns true if the whole response has been read.
  bool ReadIntoBuffer(webrtc::AsyncSocket* socket,
                      std::string* data,
                      size_t* content_length);

  void OnRead(webrtc::AsyncSocket* socket);

  void OnHangingGetRead(webrtc::AsyncSocket* socket);

  // Parses a single line entry in the form "<name>,<id>,<connected>"
  bool ParseEntry(const std::string& entry,
                  std::string* name,
                  int* id,
                  bool* connected);

  int GetResponseStatus(const std::string& response);

  bool ParseServerResponse(const std::string& response,
                           size_t content_length,
                           size_t* peer_id,
                           size_t* eoh);

  void OnClose(webrtc::AsyncSocket* socket, int err);

  void OnResolveResult(webrtc::AsyncResolverInterface* resolver);

  PeerConnectionMeshObserver* callback_;
  webrtc::SocketAddress server_address_;
  webrtc::AsyncResolver* resolver_;
  std::unique_ptr<webrtc::AsyncSocket> control_socket_;
  std::unique_ptr<webrtc::AsyncSocket> hanging_get_;
  std::string onconnect_data_;
  std::string control_data_;
  std::string notification_data_;
  std::string client_name_;

  State state_;
  int my_id_;
  
  int  sm_running;
};

#endif  // __PC_PEER_CONNECTION_CLIENT_H_
