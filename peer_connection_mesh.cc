/*
 */

#include "peer_connection_mesh.h"

#include "defaults.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/net_helpers.h"

#include "fake_audio_capture_module.h"
#include "mesh_rtc_inc.h"
#include "mesh_call.h"

//#define NAME_RESULVE_USING  1
//////////////////////////////////////////////////////////////////////////////////
namespace {

	// This is our magical hangup signal.
	const char kByeMessage[] = "BYE";
	// Delay between server connection retries, in milliseconds
	const int kReconnectDelay = 2000;

	webrtc::AsyncSocket* CreateClientSocket(int family) 
	{
#ifdef WEBRTC_POSIX
		webrtc::Thread* thread = webrtc::Thread::Current();
		RTC_DCHECK(thread != NULL);
		return thread->socketserver()->CreateAsyncSocket(family, SOCK_STREAM);
#else
#error Platform not supported.
#endif
	}

}  // namespace

//////////////////////////////////////////////////////////////////////////////////
PeerConnectionMesh::PeerConnectionMesh()
	: callback_(NULL), resolver_(NULL), state_(NOT_CONNECTED), my_id_(-1) 
{
	sm_running = 0;
}

//////////////////////////////////////////////////////////////////////////////////
PeerConnectionMesh::~PeerConnectionMesh() 
{
	webrtc::Thread::Current()->Clear(this);
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::InitSocketSignals() 
{
	RTC_DCHECK(control_socket_.get() != NULL);
	RTC_DCHECK(hanging_get_.get() != NULL);

	control_socket_->SignalCloseEvent.connect(this, &PeerConnectionMesh::OnClose);
	hanging_get_->SignalCloseEvent.connect(this, &PeerConnectionMesh::OnClose);

	control_socket_->SignalConnectEvent.connect(this, &PeerConnectionMesh::OnConnect);

	hanging_get_->SignalConnectEvent.connect(this, &PeerConnectionMesh::OnHangingGetConnect);

	control_socket_->SignalReadEvent.connect(this, &PeerConnectionMesh::OnRead);

	hanging_get_->SignalReadEvent.connect(this, &PeerConnectionMesh::OnHangingGetRead);
}

//////////////////////////////////////////////////////////////////////////////////
int PeerConnectionMesh::id() const 
{
	return my_id_;
}

//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::is_connected() const 
{
	return my_id_ != -1;
}
//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::is_logined() const 
{
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
	{
		return false;
	}		
	return true;
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::RegisterObserver(PeerConnectionMeshObserver* callback) 
{
	RTC_DCHECK(!callback_);
	callback_ = callback;
}

void PeerConnectionMesh::Connect(const std::string& server,	int port,	const std::string& client_name) 
{
	RTC_DCHECK(!server.empty());
	RTC_DCHECK(!client_name.empty());

	if ( (state_ != NOT_CONNECTED) && (state_ != SIGNING_OUT) )
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] The client must  be IDLE status before you can call Connect(). state=%d \n" , state_);
		//callback_->OnMeshServerConnectionFailure(1); // upper layer send a error cmd in this state
		return;
	}

	if (server.empty() || client_name.empty()) 
	{
		callback_->OnMeshServerConnectionFailure(2);
		return;
	}

	if (port <= 0)
		port = kDefaultServerPort;
	
	sm_running = 1;

	server_address_.SetIP(server);
	server_address_.SetPort(port);
	client_name_ = client_name;

#ifdef NAME_RESULVE_USING
	if (server_address_.IsUnresolvedIP()) 
	{
		state_ = RESOLVING;
		resolver_ = new webrtc::AsyncResolver();
		resolver_->SignalDone.connect(this, &PeerConnectionMesh::OnResolveResult);
		resolver_->Start(server_address_);
	}
	else 
#endif		
	{
		DoConnect(1);
	}
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::OnResolveResult(	webrtc::AsyncResolverInterface* resolver) 
{
#ifdef NAME_RESULVE_USING	
	if (resolver_->GetError() != 0) 
	{
		callback_->OnMeshServerConnectionFailure(3);
		resolver_->Destroy(false);
		resolver_ = NULL;
		state_ = NOT_CONNECTED; 
	}
	else 
	{
		server_address_ = resolver_->address();
		DoConnect(2);
	}
#endif	
}

/////////////////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::ServerConnFailed()
{
	MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(NOT_CONNECTED)", state_, NOT_CONNECTED);
	my_id_ = -1;	
	state_ = NOT_CONNECTED; 
}
/////////////////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::DoConnect(int reason)
{
	if(state_ == SIGNING_IN)
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] re-connect to server. reason=%d !!!", reason);
		Close(1); // release all old resource & resetup 		
		//MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "not connect to server anymore. reason=%d !!!", reason);	
		//return;
	}

	control_socket_.reset(CreateClientSocket(server_address_.ipaddr().family()));
	hanging_get_.reset(CreateClientSocket(server_address_.ipaddr().family()));

	InitSocketSignals();

	char buffer[1024];

	snprintf(buffer, sizeof(buffer), "GET /sign_in?%s HTTP/1.0\r\n\r\n", client_name_.c_str());
	onconnect_data_ = buffer;

	bool ret = ConnectControlSocket();
	if (ret)
	{	
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(SIGNING_IN)", state_, SIGNING_IN);	
		state_ = SIGNING_IN; 
	}
	else
	{
		callback_->OnMeshServerConnectionFailure(4);
	}

	webrtc::Thread::Current()->Start();
}

//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::SendToPeer(int peer_id, const std::string& message) 
{
	if (state_ != CONNECTED)
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] SendToPeer fail: state_ not CONNECTED ");
		return false;
	}
	
	RTC_DCHECK(is_connected());
	//RTC_DCHECK(control_socket_->GetState() == webrtc::Socket::CS_CLOSED);
	if(control_socket_->GetState() != webrtc::Socket::CS_CLOSED)
	{	
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] control_socket_ still exist? ");
		return true;
	}
	
	if (!is_connected() || peer_id == -1)
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] SendToPeer fail: is_connected()=%d peer_id=%d ", is_connected(), peer_id);
		return false;
	}
	
	char headers[1024];
	snprintf(headers, sizeof(headers),
		"POST /message?peer_id=%i&to=%i HTTP/1.0\r\n"
		"Content-Length: %zu\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n",
		my_id_, peer_id, message.length());
	onconnect_data_ = headers;
	onconnect_data_ += message;
	return ConnectControlSocket();
}

//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::SendHangUp(int peer_id) 
{
	return SendToPeer(peer_id, kByeMessage);
}

//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::IsSendingMessage() 
{
	return state_ == CONNECTED &&
		control_socket_->GetState() != webrtc::Socket::CS_CLOSED;
}

//////////////////////////////////////////////////////////////////////////////////
bool PeerConnectionMesh::SignOut() 
{
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
		return true;

	if(!hanging_get_ || !control_socket_)
	{
		return true;
	}

	if (hanging_get_->GetState() != webrtc::Socket::CS_CLOSED)
	{
		hanging_get_->Close();
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] hanging_get_->Close()");	
	}

	if (control_socket_->GetState() == webrtc::Socket::CS_CLOSED) 
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(SIGNING_OUT). my_id_=%d", state_, SIGNING_OUT, my_id_);	
		state_ = SIGNING_OUT; 

		if (my_id_ != -1) 
		{
			char buffer[1024];
			snprintf(buffer, sizeof(buffer), "GET /sign_out?peer_id=%i HTTP/1.0\r\n\r\n", my_id_);
			onconnect_data_ = buffer;
			my_id_          = -1;
			bool ret        = ConnectControlSocket();
			return ret;
		}
		else 
		{
			// Can occur if the app is closed before we finish connecting.
			MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(SIGNING_OUT). my_id_=%d, server conn will closed by himself?", state_, SIGNING_OUT, my_id_);	
			return true;
		}
	}
	else 
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(SIGNING_OUT_WAITING)", state_, SIGNING_OUT_WAITING);	
		state_ = SIGNING_OUT_WAITING; 
		Close(10); // TODO. send a sign_out to remote rtc server?
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::Close(int reason) 
{
	if(control_socket_) {
		control_socket_->Close();
	}
	if(hanging_get_) {
		hanging_get_->Close();
	}
	onconnect_data_.clear();
	
	if (resolver_ != NULL) 
	{
		resolver_->Destroy(false);
		resolver_ = NULL;
	}
	
	MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] [reason=%d]state_ from %d to %d(NOT_CONNECTED) my_id_=%d reset to -1", reason, state_, NOT_CONNECTED, my_id_);		
	my_id_ = -1;
	state_ = NOT_CONNECTED; 
}

bool PeerConnectionMesh::ConnectControlSocket() 
{
	RTC_DCHECK(control_socket_->GetState() == webrtc::Socket::CS_CLOSED);
	int err = control_socket_->Connect(server_address_);
	if (err == SOCKET_ERROR) {
		Close(2);
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] failed to connect to peer ");
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::OnConnect(webrtc::AsyncSocket* socket)
{
	RTC_DCHECK(!onconnect_data_.empty());
	
	size_t sent = socket->Send(onconnect_data_.c_str(), onconnect_data_.length());
	
	MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] send msg on control socket：len=%ld sent=%ld\n %s ", onconnect_data_.length(), sent, onconnect_data_.c_str());
	//RTC_DCHECK(sent == onconnect_data_.length());
	if(sent != onconnect_data_.length())
	{
		callback_->OnMeshServerConnectionFailure(5);
	}
	
	onconnect_data_.clear();
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::OnHangingGetConnect(webrtc::AsyncSocket* socket) {
	char buffer[1024];
	snprintf(buffer, sizeof(buffer), "GET /wait?peer_id=%i HTTP/1.0\r\n\r\n", my_id_);
	int len = static_cast<int>(strlen(buffer));
	int sent = socket->Send(buffer, len);
	RTC_DCHECK(sent == len);
}

//////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::OnMeshMessageFromPeer(int peer_id, const std::string& message) 
{
	if (message.length() == (sizeof(kByeMessage) - 1) && message.compare(kByeMessage) == 0)
	{
		callback_->OnMeshPeerDisconnected(peer_id);
	}
	else 
	{
		callback_->OnMeshMessageFromPeer(peer_id, message);
	}
}

bool PeerConnectionMesh::GetHeaderValue(const std::string& data,
	size_t eoh,
	const char* header_pattern,
	size_t* value) {
	RTC_DCHECK(value != NULL);
	size_t found = data.find(header_pattern);

	if (found != std::string::npos && found < eoh) {
		*value = atoi(&data[found + strlen(header_pattern)]);
		return true;
	}
	return false;
}

bool PeerConnectionMesh::GetHeaderValue(const std::string& data,	size_t eoh,
	const char* header_pattern,	std::string* value) 
{
	RTC_DCHECK(value != NULL);
	size_t found = data.find(header_pattern);
	if (found != std::string::npos && found < eoh) {
		size_t begin = found + strlen(header_pattern);
		size_t end = data.find("\r\n", begin);
		if (end == std::string::npos)
			end = eoh;
		value->assign(data.substr(begin, end - begin));
		return true;
	}
	return false;
}

bool PeerConnectionMesh::ReadIntoBuffer(webrtc::AsyncSocket* socket,	std::string* data,	size_t* content_length) 
{
	char buffer[0xffff];
	do {
		int bytes = socket->Recv(buffer, sizeof(buffer), nullptr);
		if (bytes <= 0)
			break;
		data->append(buffer, bytes);
	} while (true);

	bool ret = false;
	size_t i = data->find("\r\n\r\n");
	if (i != std::string::npos) {
		RTC_LOG(INFO) << "Headers received";
		if (GetHeaderValue(*data, i, "\r\nContent-Length: ", content_length)) {
			size_t total_response_size = (i + 4) + *content_length;
			if (data->length() >= total_response_size) {
				ret = true;
				std::string should_close;
				const char kConnection[] = "\r\nConnection: ";
				if (GetHeaderValue(*data, i, kConnection, &should_close) &&
					should_close.compare("close") == 0) {
					socket->Close();
					// Since we closed the socket, there was no notification delivered
					// to us.  Compensate by letting ourselves know.
					OnClose(socket, 0);
				}
			}
			else {
				// We haven't received everything.  Just continue to accept data.
			}
		}
		else {
			RTC_LOG(LS_ERROR) << "No content length field specified by the server.";
		}
	}
	return ret;
}

void PeerConnectionMesh::OnRead(webrtc::AsyncSocket* socket) 
{
	size_t content_length = 0;
	
	if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
	{
		return; // maybe receive a sing_out resp 
	}
	
	bool ret = ReadIntoBuffer(socket, &control_data_, &content_length);
	if (!ret) 
	{		
	    return;
	}

	size_t peer_id = 0, eoh = 0;
	bool ok = ParseServerResponse(control_data_, content_length, &peer_id, &eoh);
	if (ok) 
	{
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "++++++ control socekt OnRead: %s \n", control_data_.c_str());

		if (my_id_ == -1) 
		{
			// First response.  Let's store our server assigned ID.
			RTC_DCHECK(state_ == SIGNING_IN);
			my_id_ = static_cast<int>(peer_id);

			// The body of the response will be a list of already connected peers.
			if (content_length) {
				size_t pos = eoh + 4;
				while (pos < control_data_.size()) {
					size_t eol = control_data_.find('\n', pos);
					if (eol == std::string::npos)
						break;
					int id = 0;
					std::string name;
					bool connected;
					if (ParseEntry(control_data_.substr(pos, eol - pos), &name, &id, &connected) )
					{
						if(id == my_id_)
						{	
							callback_->OnMeshSignedIn(id, name, connected);
						} else {
							callback_->OnMeshPeerConnected(id, name);
						}
					}
					pos = eol + 1;
				}
			}
			RTC_DCHECK(is_connected());
		}
		else if (state_ == SIGNING_OUT) 
		{
			Close(3);
			callback_->OnMeshDisconnected(MESH_EVENT_LOGOUT_IND);
		}
		else if (state_ == SIGNING_OUT_WAITING) 
		{
			SignOut();
		}
	}

	control_data_.clear();

	if (state_ == SIGNING_IN) {
		RTC_DCHECK(hanging_get_->GetState() == webrtc::Socket::CS_CLOSED);
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ from %d to %d(CONNECTED)", state_, CONNECTED);			
		state_ = CONNECTED; 
		hanging_get_->Connect(server_address_);
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] state_ == SIGNING_IN & Connect  time=%lu", sys_time_get());
	}
}
	
void PeerConnectionMesh::OnHangingGetRead(webrtc::AsyncSocket* socket) 
{	
	size_t content_length = 0;
	
	if (ReadIntoBuffer(socket, &notification_data_, &content_length)) 
	{
		//printf("++++++ hanging_get_ socekt OnRead: %s \n", notification_data_.c_str());
		
		size_t peer_id = 0, eoh = 0;
		bool ok = ParseServerResponse(notification_data_, content_length, &peer_id, &eoh);	
		if (ok) 
		{
			// Store the position where the body begins.
			size_t pos = eoh + 4;

			if (my_id_ == static_cast<int>(peer_id)) 
			{
				// A notification about a new member or a member that just
				// disconnected.
				int id = 0;
				std::string name;
				bool connected = false;
				if (ParseEntry(notification_data_.substr(pos), &name, &id,	&connected)) 
				{
					if (connected) 
					{
						callback_->OnMeshPeerConnected(id, name);
					} else {
						callback_->OnMeshPeerDisconnected(id);
					}
				}
			} else {
				OnMeshMessageFromPeer(static_cast<int>(peer_id), notification_data_.substr(pos));
			}
		}

		notification_data_.clear();
	}

	if (hanging_get_->GetState() == webrtc::Socket::CS_CLOSED &&
		state_ == CONNECTED) {
		hanging_get_->Connect(server_address_);
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] hanging_get_->GetState() == webrtc::Socket::CS_CLOSED & Connect ");	
	}
}

bool PeerConnectionMesh::ParseEntry(const std::string& entry,
	std::string* name,
	int* id,
	bool* connected) {
	RTC_DCHECK(name != NULL);
	RTC_DCHECK(id != NULL);
	RTC_DCHECK(connected != NULL);
	RTC_DCHECK(!entry.empty());

	*connected = false;
	size_t separator = entry.find(',');
	if (separator == std::string::npos) {
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "decode msg error: no first , ");
		return false;
	}

	*id = atoi(&entry[separator + 1]);
	name->assign(entry.substr(0, separator));
	separator = entry.find(',', separator + 1);
	if (separator == std::string::npos) {
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "decode msg error: no second , ");
		return false;
	}		
	*connected = atoi(&entry[separator + 1]) ? true : false;
	
	return !name->empty();
}

int PeerConnectionMesh::GetResponseStatus(const std::string& response) {
	int status = -1;
	size_t pos = response.find(' ');
	if (pos != std::string::npos)
		status = atoi(&response[pos + 1]);
	return status;
}

bool PeerConnectionMesh::ParseServerResponse(const std::string& response,
	size_t content_length,
	size_t* peer_id,
	size_t* eoh) {
		
	int status = GetResponseStatus(response.c_str());
	if (status != 200) {
		MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "Received error from server: %s", response.c_str());
		Close(4);
		if(strstr(response.c_str(), "busy"))
		{
			callback_->OnMeshDisconnected(MESH_EVENT_PEER_BUSY);
		} else {
			callback_->OnMeshDisconnected(MESH_EVENT_LOGIN_FAIL);
		}
		return false;
	}

	*eoh = response.find("\r\n\r\n");
	RTC_DCHECK(*eoh != std::string::npos);
	if (*eoh == std::string::npos)
		return false;

	*peer_id = -1;

	// See comment in peer_channel.cc for why we use the Pragma header.
	GetHeaderValue(response, *eoh, "\r\nPragma: ", peer_id);

	return true;
}

void PeerConnectionMesh::OnClose(webrtc::AsyncSocket* socket, int err) 
{
	socket->Close();

	if (err != ECONNREFUSED) {

		if (socket == hanging_get_.get()) {
			if (state_ == CONNECTED) 
			{
				hanging_get_->Close();
				hanging_get_->Connect(server_address_);
				MESH_LOG_PRINT(PC_MESH_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] hanging_get_->Close() & re-Connect to server  time=%lu", sys_time_get());	
			}
		}
		else {
			callback_->OnMeshMessageSent(err);
		}
	}
	else 
	{
		if (socket == control_socket_.get()) 
		{
			// retrying only in this state
			if(sm_running)
			{
				//rtc send msg to PeerConnectionMesh::OnMessage. Upper layer keep in PC_CONN_MESH_LOGINING status 
				RTC_LOG(WARNING) << "Connection refused; retrying in 2 seconds";
				webrtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, kReconnectDelay, this, 0);
				MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] Connection refused; retrying in %d ms", kReconnectDelay);
			}
		}
		else 
		{
			Close(5);
			callback_->OnMeshDisconnected(MESH_EVENT_DISCONN_IND);
			//rtc send msg to PeerConnectionMesh::OnMessage. Upper layer keep in PC_CONN_MESH_LOGINING status 			
			webrtc::Thread::Current()->PostDelayed(RTC_FROM_HERE, kReconnectDelay, this, 0);
			MESH_LOG_PRINT(CONN_CTRL_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] Connection lost; retrying in %d ms", kReconnectDelay);			
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::OnMessage(webrtc::Message* msg) 
{
	// ignore msg; there is currently only one supported message ("retry")
	if(sm_running)
	{
		DoConnect(3);
	}
}
//////////////////////////////////////////////////////////////////////////////////////////
void PeerConnectionMesh::Start()
{
	sm_running = 1;
}

void PeerConnectionMesh::Stop()
{
	sm_running = 0;
}
