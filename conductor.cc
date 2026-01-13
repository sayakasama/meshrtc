/*
 */

#include "conductor.h"

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"

#include "media/engine/fake_webrtc_video_engine.h"
#include "media/engine/fake_video_codec_factory.h"

#include "./defaults.h"

#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "rtc_base/strings/json.h"
#include "test/vcm_capturer.h"

#include "video/fake_video_capture.h"
#include "audio/fake_audio_capture_module.h"
#include "fake_video_render.h"          

#include "mesh_rtc_inc.h"
#include "encoder/external_video_encoder_factory.h"

#include "cjson.h"
#include "cfg.h"
#include "av_api.h"
#include "../av_cfg.h"
#include "utils.h"
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
namespace {
	// Names used for a IceCandidate JSON object.
	const char kCandidateSdpMidName[] = "sdpMid";
	const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
	const char kCandidateSdpName[] = "candidate";

	// Names used for a SessionDescription JSON object.
	const char kSessionDescriptionTypeName[] = "type";
	const char kSessionDescriptionSdpName[]  = "sdp";

	class DummySetSessionDescriptionObserver
		: public webrtc::SetSessionDescriptionObserver {
	public:
		static DummySetSessionDescriptionObserver* Create() {
			return new webrtc::RefCountedObject<DummySetSessionDescriptionObserver>();
		}
		virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
		virtual void OnFailure(webrtc::RTCError error) {
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " error type=%d error msg=%s" , (int)error.type(), error.message());
		}
	};

}  // namespace
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
webrtc::scoped_refptr<FAKE_VIDEO::FakeVideoCapturer>      video_device_1 = NULL;
webrtc::scoped_refptr<FAKE_AUDIO::FakeAudioCaptureModule> audio_device   = NULL;

int video_simucast_num = 1;

bool dtls_off = false; // false, we always turn on dtls. If set true , ZLMedia server will reject.

extern av_params_t    av_params;

// 0:质量优先 1:流畅优先 值的获取在rtc_main.cc周期性更新
extern int   g_rcmode;

std::string sdpRotationLineDel(std::string& sdp);
std::string forceVideoBandwidth(std::string& sdp, int bitrate_kbps = 30000);
std::string RemoveTransportCC(const std::string& sdp);
/////////////////////////////////////////////////////////////////////////
Conductor::Conductor(PeerConnectionMesh* client_m, PeerConnectionClient* client_c, MainWindow* main_wnd, const char *peer_ip, const char *local_ip, int samples)
	: peer_id_(-1), /*loopback_(false),*/ video_call_(true), video_local_src_(false), video_server_type_(0), client_m_(client_m), client_c_(client_c), main_wnd_(main_wnd) 
{
	pcm_sampling_rate_ = samples;

	client_m_->RegisterObserver(this);
	client_c_->RegisterObserver(this);
	main_wnd->RegisterObserver(this);	
}

Conductor::~Conductor() {
	RTC_DCHECK(!peer_connection_);
}

bool Conductor::connection_active() const {
	return peer_connection_ != nullptr;
}

void Conductor::Close() {
	client_m_->SignOut();
	DeletePeerConnection();
}

bool Conductor::InitializePeerConnection(bool wh_limited) 
{
	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "InitializePeerConnection: peer_connection_factory_=%0x peer_connection_=%0x", peer_connection_factory_, peer_connection_);

	RTC_DCHECK(!peer_connection_factory_);
	RTC_DCHECK(!peer_connection_);
	
	// create audio device module 
	// use I/O thread as audio I/O 
	webrtc::scoped_refptr<FAKE_AUDIO::FakeAudioCaptureModule> audio_device;
	if(wh_limited)
	{
		// wh_limited means mesh call and we use file as audio I/O . In fact, these audio files do not exist.
		// So no audio data is transfered between caller/calle. At last user must use ptt to talk.
		audio_device = FAKE_AUDIO::FakeAudioCaptureModule::Create(true, pcm_sampling_rate_); 
	} else {
		audio_device = FAKE_AUDIO::FakeAudioCaptureModule::Create(false, pcm_sampling_rate_); 
	}
	
	if(video_local_src_)
	{
		peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
		nullptr /* network_thread */, nullptr /* worker_thread */,
		nullptr /* signaling_thread */, 
		audio_device,  
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(), 
		nullptr /* audio_mixer */,
		nullptr /* audio_processing */);		
	} else {
		peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
		nullptr /* network_thread */, nullptr /* worker_thread */,
		nullptr /* signaling_thread */, 
		audio_device,  
		webrtc::CreateBuiltinAudioEncoderFactory(),
		webrtc::CreateBuiltinAudioDecoderFactory(),
		webrtc::CreateBuiltinExternalVideoEncoderFactory(),
		webrtc::CreateBuiltinVideoDecoderFactory(), 
		nullptr /* audio_mixer */,
		nullptr /* audio_processing */);
	}
	
	if (!peer_connection_factory_) {
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "Failed to initialize PeerConnectionFactory");
		DeletePeerConnection();
		return false;
	}

   webrtc::PeerConnectionFactoryInterface::Options options;
   if(wh_limited) {
		options.disable_encryption = true; // not encypt to save conn time
   } else {
		options.disable_encryption = dtls_off;
   }
   peer_connection_factory_->SetOptions(options);

	if (!CreatePeerConnection(dtls_off, wh_limited)) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "CreatePeerConnection failed");
		DeletePeerConnection();
	}
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "CreatePeerConnection ok");

	AddTracks();

	return peer_connection_ != nullptr;
}

/////////////////////////////////////////////////////////////////////////
void AudioSamplesChange(int samples)
{
	if(!audio_device)
	{
		return;
	}
		
	audio_device->AudioSamplesChange(samples);
}
/////////////////////////////////////////////////////////////////////////
bool Conductor::CreatePeerConnection(bool dtls_disable, bool wh_limited) 
{
	RTC_DCHECK(peer_connection_factory_);
	RTC_DCHECK(!peer_connection_);

	webrtc::PeerConnectionInterface::RTCConfiguration config;
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
#if  1	
   	config.enable_dtls_srtp = !dtls_disable;
	// not setup ice server within mesh network	
#else 
	config.enable_dtls_srtp = dtls;
	webrtc::PeerConnectionInterface::IceServer server;
	server.uri = GetPeerConnectionString();
	config.servers.push_back(server);
#endif

	peer_connection_ = peer_connection_factory_->CreatePeerConnection(config, nullptr, nullptr, this);
	return peer_connection_ != nullptr;
}

void Conductor::DeletePeerConnection() 
{
	main_wnd_->StopLocalRenderer();
	main_wnd_->StopRemoteRenderer();

	peer_connection_ = nullptr;
	peer_connection_factory_ = nullptr;
	
	//peer_id_ = -1; // peer is still here
}

/////////////////////////////////////////////////////////////////////////
void Conductor::EnsureStreamingUI() {
	RTC_DCHECK(peer_connection_);
#ifdef MAIN_WND_USING	
	if (main_wnd_->IsWindow()) 
	{
		if (main_wnd_->current_ui() != MainWindow::STREAMING)
			main_wnd_->SwitchToStreamingUI();
	}
#endif	
}

//
// PeerConnectionObserver implementation.
//
/////////////////////////////////////////////////////////////////////////
void Conductor::OnConnectionChange(webrtc::PeerConnectionInterface::PeerConnectionState new_state)
{
	// In fact this state change is not called 
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  " ++++++++++++++++++++++++ connect status: %d ++++++++++++++++++++++++ ", (int)new_state);
	
	if(webrtc::PeerConnectionInterface::PeerConnectionState::kConnected == new_state)
	{
		av_params_t    encoder_av_params;
		av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;

		memcpy(&encoder_av_params, &av_params, sizeof(av_params));
	
		if(video_server_type_ == 0){
			video_cfg->gop    = 20; // key frame as soon as quickly
			video_cfg->fps    = 20;
			video_cfg->w      = 1280; 
			video_cfg->h      = 720;
			video_cfg->qp     = 26;
			video_cfg->code_rate = 1200*1000;
		}
		else{
			video_cfg->gop    = 100; // key frame as soon as quickly
			video_cfg->fps    = 20;
			video_cfg->w      = 1280; 
			video_cfg->h      = 720;
			video_cfg->qp     = 26;
			video_cfg->code_rate = 1200*1000;
		}	
		// trigger a video fragment 
		video_cfg_set((char *)&encoder_av_params, sizeof(encoder_av_params));
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Init av module encoder param and trigger a video frame as soon as quickly on ICE kConnected status");
	}
	
	if ((webrtc::PeerConnectionInterface::PeerConnectionState::kClosed == new_state) || (webrtc::PeerConnectionInterface::PeerConnectionState::kDisconnected == new_state))
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Our peer %d disconnected or power off", peer_id_);
		if(video_server_type_ != 0)
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "XV9200 SERVER CHANGE TO DISCONNECTED...");
			system("echo 0 > /tmp/rtc_server_conn");
		}
		main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
		if(peer_id_ != -1)
		{
			PeerIdSet(-1, 3); // clear peer 	
		}
		client_c_->WebrtcStreamClean();
		return;
	}
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state)
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  " ++++++++++++++++++++++++ ICE connect status: %d ++++++++++++++++++++++++ ", (int)new_state);
	
	if(webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed == new_state)
    {
		// do nothing
	}
	
	if(webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected == new_state)
	{	
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Our peer %d disconnected or power off", peer_id_);
		
		if(video_server_type_ != 0)
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "XV9200 SERVER CHANGE TO DISCONNECTED...");
			system("echo 0 > /tmp/rtc_server_conn");
		}
		main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
		if(peer_id_ != -1)
		{
			PeerIdSet(-1, 3); // clear peer 	
		}
	}
	return;
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnAddTrack(
	webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
	const std::vector<webrtc::scoped_refptr<webrtc::MediaStreamInterface>>&  streams) 
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  " add trackId: %s" ,receiver->id().c_str());
	main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED, receiver->track().release());
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnRemoveTrack(
	webrtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) 
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  " remove trackId: %s" ,receiver->id().c_str());
	main_wnd_->QueueUIThreadCallback(TRACK_REMOVED, receiver->track().release());
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) 
{
	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "sdp_mline_index=%s ", candidate->sdp_mline_index());
	
	Json::StreamWriterBuilder builder;
	std::ostringstream os;
	Json::Value jmessage;		

	jmessage[kCandidateSdpMidName] = candidate->sdp_mid();
	jmessage[kCandidateSdpMlineIndexName] = candidate->sdp_mline_index();
	std::string sdp;
	if (!candidate->ToString(&sdp)) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Failed to serialize candidate");
		return;
	}
	if(strstr(sdp.c_str(), "169.167.") /*|| strstr(sdp.c_str(), "tcptype")*/) //TODO. shangtao. To limit internal ip & tcp protocols
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "discard internal port sdp: %s", sdp.c_str());
		return;
	} else {
		//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "ICE  candidate : %s", sdp.c_str());
	}
	
	jmessage[kCandidateSdpName] = sdp;
		
	std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
	writer->write(jmessage, &os);
	SendMessage(os.str());
}

//
//PeerConnectionMeshObserver implementation.
//
/////////////////////////////////////////////////////////////////////////
void Conductor::OnMeshSignedIn(int id, const std::string& name, bool connected)
{
	main_wnd_->OnMeshSignedIn(id, name, connected);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnMeshDisconnected(int reason) {
	RTC_LOG(INFO) << __FUNCTION__;

	if(video_device_1)
	{
		video_device_1->StopCapture();
		delete video_device_1;
		video_device_1 = NULL;
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "[OnMeshDisconnected]video_device_1 stop to capture photo...reason=%d \n", reason);
	}
	if(audio_device)
	{
		audio_device = NULL;
	}
	
	DeletePeerConnection();
#ifdef MAIN_WND_USING	
	if (main_wnd_->IsWindow())
		main_wnd_->SwitchToConnectUI();
#else
	main_wnd_->OnMeshDisconnected(reason); // keep conn with rtc_server
#endif	
}

void Conductor::PeerIdSet(int id, int reason) 
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " change peer_id_ from %d to %d reason=%d \n", peer_id_, id, reason);		
	peer_id_ = id;
}
	
void Conductor::OnMeshPeerConnected(int id, const std::string& name) {
	
	RTC_LOG(INFO) << __FUNCTION__;
		
#ifdef MAIN_WND_USING	
	// Refresh the list if we're showing it.
	if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
		main_wnd_->SwitchToPeerList(client_m_->peers());
#else
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " id=%d %s online \n", id, name.c_str());
	//PeerIdSet(id, 4);
	main_wnd_->OnMeshPeerConnected(id, name);
	
#endif	
}

void Conductor::OnMeshPeerDisconnected(int id) {

	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "input id=%d and local peer_id_=%d\n", id, peer_id_);

	if (id == peer_id_) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Our peer %d disconnected", peer_id_);
		main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
		PeerIdSet(-1, 5); // clear peer 
	}
	else 
	{
#ifdef MAIN_WND_USING		
		// Refresh the list if we're showing it.
		if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
			main_wnd_->SwitchToPeerList(client_m_->peers());
#
		// do nothing
#endif		
	}
	
	//peer_id_ = -1; // clear peer 
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnMeshMessageFromPeer(int peer_id, const std::string& message) 
{
	RTC_DCHECK(!message.empty());

	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " OnMeshMessageFromPeer peer_id=%d while local peer_id_=%d . message=%s \n", peer_id, peer_id_, message.c_str());

	if (!peer_connection_.get()) {
		//RTC_DCHECK(peer_id_ == -1);
		PeerIdSet(peer_id, 7);

		if (!InitializePeerConnection(true)) 
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Failed to initialize our PeerConnection instance");
			client_m_->SignOut();
			return;
		}
	}
	else if (peer_id != peer_id_) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Received a message from unknown peer %d while already in a conn with a diff peer %d.", peer_id, peer_id_);
		return;
	}
	
	bool res;
    JSONCPP_STRING errs;
    Json::Value jmessage;
    Json::CharReaderBuilder readerBuilder; 
    std::unique_ptr<Json::CharReader> const jsonReader(readerBuilder.newCharReader());
    res = jsonReader->parse(message.c_str(), message.c_str()+message.length(), &jmessage, &errs);
    if (!res || !errs.empty()) {
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Received unknown message. %s ", message.c_str());
		return;
    }

	std::string type_str = "";
	std::string json_object;

	webrtc::GetStringFromJsonObject(jmessage, kSessionDescriptionTypeName, &type_str);
  
	if (!type_str.empty()) {

		absl::optional<webrtc::SdpType> type_maybe = webrtc::SdpTypeFromString(type_str);
		if (!type_maybe) 
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Unknown SDP type: ");// (int)type_str);
			return;
		}
		webrtc::SdpType type = *type_maybe;
		std::string sdp;

		if (!webrtc::GetStringFromJsonObject(jmessage, kSessionDescriptionSdpName, &sdp)) 
		{
		  MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Can't parse received session description message.");
		  return;
		}
		
		webrtc::SdpParseError error;
		std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =	webrtc::CreateSessionDescription(type, sdp, &error);
		if (!session_description) 
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "Can't parse received session description message. SdpParseError was: %s \n %s", error.description.c_str(), error.line.c_str());
			return;
		}
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " Received session description & SetRemoteDescription ");
		peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description.release());
		if (type == webrtc::SdpType::kOffer) 
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " Received sdp offer type & CreateAnswer");
			peer_connection_->CreateAnswer(	this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
		}
	}
	else 
	{
		std::string sdp_mid;
		int sdp_mlineindex = 0;
		std::string sdp;

		if (!webrtc::GetStringFromJsonObject(jmessage, kCandidateSdpMidName,  &sdp_mid) ||
			!webrtc::GetIntFromJsonObject(jmessage, kCandidateSdpMlineIndexName, &sdp_mlineindex) ||
			!webrtc::GetStringFromJsonObject(jmessage, kCandidateSdpName, &sdp))
		{
		  MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "Can't parse received message.");
		  return;
		}
		
		webrtc::SdpParseError error;
		std::unique_ptr<webrtc::IceCandidateInterface> candidate(
			webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
		if (!candidate.get()) 
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Can't parse received candidate message. SdpParseError was: %s", error.description.c_str());
			return;
		}
		if (!peer_connection_->AddIceCandidate(candidate.get())) {
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Failed to apply the received candidate");
			return;
		}
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " Received candidate :%s",  message.c_str());
	}
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnMeshMessageSent(int err) 
{
	// Process the next pending message if any.
	main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnMeshServerConnectionFailure(int reason) 
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "Failed to connect to %s. reason=%d",  server_.c_str(), reason);
	main_wnd_->OnMeshSignFailed();
	client_m_->ServerConnFailed();
}

//
// MainWndCallback implementation.
//
/////////////////////////////////////////////////////////////////////////
void Conductor::StartLogin(const std::string& server, int port, const char *name) 
{
	if (client_m_->is_logined())
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "[PeerConnectionMesh] mesh client is logined on %s ? please logout before login!", server_.c_str());
		return;
	}
	
	server_ = server;
	client_m_->Start();
	client_m_->Connect(server, port, name);
}

void Conductor::DisconnectFromServer() 
{
	if (client_m_->is_logined())
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] SignOut from server %s", server_.c_str());
		client_m_->SignOut();
	} else {
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "[PeerConnectionMesh] Close conn to server %s", server_.c_str());	
		client_m_->Close(10);
	}
	client_m_->Stop();
	if(-1 != peer_id_)
	{
		PeerIdSet(-1, 10); // clear peer if pc mesh conn closed
	}
}

/////////////////////////////////////////////////////////////////////////
/*
void Conductor::ConnectedPeerResult(bool ret)
{
	
}*/
/////////////////////////////////////////////////////////////////////////
void Conductor::ConnectToPeer(int peer_id) 
{
	if (peer_id == -1) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "peer_id is invalid -1 ");
		return;
	}

	if (peer_connection_.get()) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "We only support connecting to one peer at a time");
		return;
	}

	if (InitializePeerConnection(true)) 
	{
		PeerIdSet(peer_id, 8);
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions option;
		option.num_simulcast_layers = video_simucast_num;
		peer_connection_->CreateOffer(this, option);
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "ConnectToPeer: CreateOffer peer_id=%d \n", peer_id);
	}
	else 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "Failed to initialize PeerConnection");
	}
}

/////////////////////////////////////////////////////////////////////////
void Conductor::AddTracks() 
{
	if (!peer_connection_) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "peer_connection_ is null ");
		return;  // invalid
	}
	
	if (!peer_connection_->GetSenders().empty()) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, "peer_connection_->GetSenders().empty() ");		
		return;  // Already added tracks.
	}

	webrtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
		peer_connection_factory_->CreateAudioTrack(
			kAudioLabel, peer_connection_factory_->CreateAudioSource(cricket::AudioOptions())));
	auto result_or_error = peer_connection_->AddTrack(audio_track, { kStreamId });
	if (!result_or_error.ok()) {
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Failed to add audio track to PeerConnection: %s " , result_or_error.error().message());
	}

	// create video track 
	if(video_call_)
	{
		int fps;
		if(video_local_src_) {
			fps = 3; // fake video use a small fps 
		} else {
			av_video_fps_get(&fps);
		}
		webrtc::scoped_refptr<FAKE_VIDEO::FakeVideoCapturer> video_device = FAKE_VIDEO::FakeVideoCapturer::Create(fps);
		video_device_1 = video_device;
		if (video_device) 
		{
			webrtc::scoped_refptr<webrtc::VideoTrackInterface> video_track_(
				peer_connection_factory_->CreateVideoTrack(kVideoLabel, video_device));

			 //shangtao add 20240521: we setup WebRTC-Video-BalancedDegradation when startup
			//enum class ContentHint { kNone, kFluid, kDetailed, kText };
			video_track_->set_content_hint(webrtc::VideoTrackInterface::ContentHint::kFluid);

			result_or_error = peer_connection_->AddTrack(video_track_, { kStreamId });
			if (!result_or_error.ok()) 
			{
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Failed to add video track to PeerConnection: %s" , result_or_error.error().message());
			}
			else 
			{
				video_device->StartCapture(video_local_src_);
			}
		} else {
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "OpenVideoCaptureDevice failed");
		}
	}

	main_wnd_->SwitchToStreamingUI();
}

/////////////////////////////////////////////////////////////////////////
void Conductor::DisconnectFromCurrentPeer(int status) {
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,"conn_ctrl status=%d and disconn ...peer_id_=%d", status, peer_id_);
	
	if (peer_connection_ && peer_connection_.get()) 
	{
		if(peer_id_ != -1)
		{
			bool ret = client_m_->SendHangUp(peer_id_);
			if(!ret) {
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO," send hangup message to peer failed ");
			}
			sleep_ms(300); // wait for msg sending
		} else {
			client_c_->WebrtcStreamClean(); // clean stream info to server
		}
		DeletePeerConnection();
	}
	
	if(video_device_1)
	{
		video_device_1->StopCapture();
		video_device_1 = NULL;
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "video_device_1 stop to capture photo...\n");
	}
	
}

/////////////////////////////////////////////////////////////////////////
void Conductor::UIThreadCallback(int msg_id, void* data) 
{
	
	switch (msg_id) 
	{
	case PEER_CONNECTION_CLOSED:
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "PEER_CONNECTION_CLOSED");
		DeletePeerConnection();

#ifdef MAIN_WND_USING					
		if (main_wnd_->IsWindow()) 
		{
			if (client_m_->is_connected()) 
			{
				main_wnd_->SwitchToPeerList(client_m_->peers());
			}
			else 
			{
				main_wnd_->SwitchToConnectUI();
			}
		}
#else
		if(1)
		{
			main_wnd_->OnMeshPeerDisconnected();
			if(video_device_1)
			{
				video_device_1->StopCapture();
				video_device_1 = NULL;
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "video_device_1 stop to capture photo...\n");
			}

		}
#endif				
		else 
		{
			DisconnectFromServer();
		}
		break;

	case SEND_MESSAGE_TO_PEER: {
		//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "SEND_MESSAGE_TO_PEER");
		std::string* msg = reinterpret_cast<std::string*>(data);
		if (msg) {
			// For convenience, we always run the message through the queue.
			// This way we can be sure that messages are sent to the server
			// in the same order they were signaled without much hassle.
			pending_messages_.push_back(msg);
		}

		if (!pending_messages_.empty() && !client_m_->IsSendingMessage()) 
		{
			msg = pending_messages_.front();
			pending_messages_.pop_front();

			if (!client_m_->SendToPeer(peer_id_, *msg) && peer_id_ != -1) {
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "SendToPeer failed? peer_id_=%d ", peer_id_);
			}
			delete msg;
		}

		if (!peer_connection_.get())
		{
			//PeerIdSet(-1, 9);
		}
		
		break;
	}

	case NEW_TRACK_ADDED: {
		auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
		if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) 
		{
			auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
			main_wnd_->StartRemoteRenderer(video_track);
		}
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "track->kind()=%s kVideoKind=%s ", (track->kind()).c_str(), webrtc::MediaStreamTrackInterface::kVideoKind);
		track->Release();
		// connection is ok
		main_wnd_->ConnectedPeerResult(true);
		break;
	}

	case TRACK_REMOVED: {
		// Remote peer stopped sending a track.
		auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
		track->Release();
		break;
	}

	default:
		RTC_NOTREACHED();
		break;
	}
}

//webrtc::SessionDescriptionInterface local_desc;
/////////////////////////////////////////////////////////////////////////
void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) 
{
	peer_connection_->SetLocalDescription(DummySetSessionDescriptionObserver::Create(), desc);

	std::string sdp;
	desc->ToString(&sdp);
	if(g_rcmode != RC_MODE_FLUENCY_FIRST_MODE)
	{
		sdp = forceVideoBandwidth(sdp,30000);
	}

	if(peer_id_ == -1)
	{
		// sdp to rtc server
		// for test & debug. delete it in release version . shangtao !!!!
		//sdp = sdpMsgModify(sdp);		
		
		std::string* msg = new std::string(sdp);	
		main_wnd_->QueueUIThreadCallback(WEBRTC_SERVER_MESSAGE_SEND, msg);
	}
	else
	{
		// sdp to mesh peer
		Json::StreamWriterBuilder builder;
		std::ostringstream os;
		Json::Value jmessage;		
		jmessage[kSessionDescriptionTypeName] = webrtc::SdpTypeToString(desc->GetType());
	
#ifdef BUILTIN_CODEC_TEST // not modify
		//sdp = sdpMsgModify(sdp); // delete it in release version !!!!		
		//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "create sdp: \n%s", sdp.c_str());
#else
		sdp = sdpRotationLineDel(sdp);	
#endif	
		jmessage[kSessionDescriptionSdpName]  = sdp;
		
		std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
		writer->write(jmessage, &os);
		SendMessage(os.str());
	}
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnFailure(webrtc::RTCError error) 
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "type=%d error msg=%s", (int)error.type(), error.message());
}

/////////////////////////////////////////////////////////////////////////
void Conductor::SendMessage(const std::string& json_object) 
{
	std::string* msg = new std::string(json_object);
	main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
}

/////////////////////////////////////////////////////////////////////////
void Conductor::video_enable(bool enable)
{
	video_call_ = enable;	
}
/////////////////////////////////////////////////////////////////////////
void Conductor::local_video_use(bool local_src)
{
	video_local_src_ = local_src;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Conductor::OnClientSignedIn()
{
	std::string* msg = new std::string("login_ok");
	main_wnd_->QueueUIThreadCallback(WEBRTC_SERVER_LOGINED, msg);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientSignedOut()
{
	std::string* msg = new std::string("login_out");	
	main_wnd_->QueueUIThreadCallback(WEBRTC_SERVER_LOGOUT, msg);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientPeerConnected()
{

}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientPeerDisconnected()
{

}
/////////////////////////////////////////////////////////////////////////
int Conductor::OnClientMessageFromPeer(const std::string& sdp)
{
	if(peer_id_ != -1)
	{
		return -1; // local connection exist?
	}

	if(sdp.empty())
	{
		return -1;
	}

	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " InitializePeerConnection peer_id=%d message=%s \n", peer_id, message.c_str());
    
	// Normallly we init connection first
	if (!peer_connection_.get()) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "local pc not inited ?");
		return -1;
	}
	
	webrtc::SdpType type = webrtc::SdpType::kAnswer;	
	webrtc::SdpParseError error;
	std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =	webrtc::CreateSessionDescription(type, sdp, &error);
	if (!session_description) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "Can't parse received session description message. SdpParseError was: %s", error.description.c_str());
		return -1;
	}

	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " Received sdp from peer : \n%s" , sdp.c_str());

	peer_connection_->SetRemoteDescription(DummySetSessionDescriptionObserver::Create(), session_description.release());
	
	return 1;
}

/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientCalled(bool audio_enable, bool video_enable)
{
	std::string audio_on = "audio=" + std::to_string(audio_enable);
	std::string video_on = "video=" + std::to_string(video_enable);
	
	std::string* msg = new std::string(audio_on + " " + video_on);	
	main_wnd_->QueueUIThreadCallback(WEBRTC_SERVER_CALL, msg);	// shangtao--
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientHangup()
{
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "OnClientHangup ");
	std::string* msg = new std::string("hangup");		
	main_wnd_->QueueUIThreadCallback(WEBRTC_SERVER_HANGUP, msg);	// shangtao--
}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientMessageSent(int err)
{

}
/////////////////////////////////////////////////////////////////////////
void Conductor::OnClientServerConnectionFailure() 
{

}
/////////////////////////////////////////////////////////////////////////
void Conductor::WebrtcServerSet(const std::string& addr, const std::string& port, const std::string& port_s)
{
	client_c_->WebrtcServerSet(addr, port, port_s);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::WebrtcMediaSet(const std::string& addr, const std::string& port, const std::string& port_s)
{
	client_c_->WebrtcMediaSet(addr, port, port_s);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::StartWebrtcLogin(const std::string& user, const std::string& passwd, const std::string& video_id) 
{
	client_c_->Login(user, passwd, video_id);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::StartWebrtcLogout() 
{
	client_c_->Logout();
}
/////////////////////////////////////////////////////////////////////////
void Conductor::SetAppName(const std::string& app_name)
{
	client_c_->SetAppName(app_name);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::SetStreamID(const std::string& user)
{
	client_c_->SetStreamID(user);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::SetServerType(int server_type)
{
	video_server_type_ = server_type;
}
/////////////////////////////////////////////////////////////////////////
int Conductor::GetServerStatusFlag()
{
	if(peer_connection_.get() && client_c_->peer_server_check_times_ > 3)
	{
		client_c_->peer_server_check_times_ = 0;
		return -1;
	}
	else if(peer_connection_.get())
	{
		return 2;
	}
	return client_c_->peer_server_status_;
}
/////////////////////////////////////////////////////////////////////////
void Conductor::SetServerStatusFlag(int server_status_flag)
{
	client_c_->SetPeerServerStatus(server_status_flag);
}
/////////////////////////////////////////////////////////////////////////
void Conductor::GetInnerServerStatus()
{
	client_c_->GetInnerServerStatus();
}
/////////////////////////////////////////////////////////////////////////
void Conductor::SendWebrtcSdp(const std::string& sdp)
{
	client_c_->SdpMessageSend(sdp);
	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " %s \n", sdp.c_str());
}
/////////////////////////////////////////////////////////////////////////
void Conductor::ConnectToRtcServer() 
{
	if (peer_connection_.get()) 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "ConnectToRtcServer only support connecting to one server at a time");
		return;
	}

	if (InitializePeerConnection(false)) 
	{
		client_c_->SetPeerServerType(video_server_type_);
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions option;
		option.num_simulcast_layers = video_simucast_num;
		peer_connection_->CreateOffer(this, option);
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "ConnectToRtcServer: CreateOffer  \n");
	}
	else 
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO,  "ConnectToRtcServer Failed to initialize PeerConnection");
	}
}
/////////////////////////////////////////////////////////////////////////
std::string sdpRotationLineDel(std::string& sdp)
{
	// Not rotation image 
	// delete line:  a=extmap:13 urn:3gpp:video-orientation\r\n	
	size_t pos1 = sdp.find("urn:3gpp:video-orientation");
	if (pos1 == std::string::npos) 
	{
		return sdp;
	}
		
	size_t pos3 = sdp.find("a=extmap", pos1 - 16); // line head
	size_t pos4 = sdp.find("a=extmap", pos1);	// next line head
	if((pos3 != std::string::npos) && (pos4 != std::string::npos))
	{
		sdp.replace(pos3, pos4 - pos3, ""); // delete this line
	}

	return sdp;	
}

/////////////////////////////////////////////////////////////////////////
std::string Conductor::sdpMsgModify(std::string& sdp)
{

#if 0	
	size_t pos1 = sdp.find("m=video");
	if (pos1 == std::string::npos) 
	{
		return sdp;
	}
		
	//m=video 9 UDP/TLS/RTP/SAVPF 96 97 98 99 100 101 102 123 127 122 125
	//m=video 9 UDP/TLS/RTP/SAVPF       98 99 100 101 102 123 127 122 125
	size_t pos3 = sdp.find("96", pos1);
	size_t pos4 = sdp.find("97", pos1);
	if((pos3 != std::string::npos) && (pos4 != std::string::npos))
	{
		sdp.replace(pos4, 3, ""); // 97+space
		sdp.replace(pos3, 3, ""); // 96+space
	}

	//a=rtpmap:96
	while(1)
	{
		size_t pos_n = sdp.find("a=rtpmap:96", pos1);
		if(pos_n == std::string::npos)
		{
			break;
		}
		size_t pos_m = sdp.find("a=rtpmap:98", pos1);
		if(pos_m == std::string::npos)
		{
			break;
		}		
		sdp.replace(pos_n, pos_m - pos_n, "");
		break;
	}
	
//////////////////////////////////////
/*
a=rtpmap:102 H264/90000
a=rtcp-fb:102 goog-remb
a=rtcp-fb:102 transport-cc
a=rtcp-fb:102 ccm fir
a=rtcp-fb:102 nack
a=rtcp-fb:102 nack pli
a=fmtp:102 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
a=rtpmap:124 rtx/90000
a=fmtp:124 apt=102
a=rtpmap:127 H264/90000
a=rtcp-fb:127 goog-remb
a=rtcp-fb:127 transport-cc
a=rtcp-fb:127 ccm fir
a=rtcp-fb:127 nack
a=rtcp-fb:127 nack pli
a=fmtp:127 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
a=rtpmap:123 rtx/90000
a=fmtp:123 apt=127
a=rtpmap:125 red/90000
*/
	pos3 = sdp.find("102", pos1);
	pos4 = sdp.find("124", pos1);
	if((pos3 != std::string::npos) && (pos4 != std::string::npos))
	{
		sdp.replace(pos4, 4, ""); // 102+space
		sdp.replace(pos3, 4, ""); // 124+space
	}

	pos3 = sdp.find("100", pos1);
	pos4 = sdp.find("101", pos1);
	if((pos3 != std::string::npos) && (pos4 != std::string::npos))
	{
		sdp.replace(pos4, 4, ""); // 102+space
		sdp.replace(pos3, 4, ""); // 124+space
	}
	
	pos3 = sdp.find("127", pos1);
	pos4 = sdp.find("123", pos1);
	if((pos3 != std::string::npos) && (pos4 != std::string::npos))
	{
		sdp.replace(pos4, 4, ""); // 102+space
		sdp.replace(pos3, 4, ""); // 124+space
	}
	
	size_t pos_n = sdp.find("a=rtpmap:100", pos1);
	size_t pos_m = sdp.find("a=ssrc-group:", pos1);
	if((pos_n != std::string::npos) && (pos_m != std::string::npos))
	{
		sdp.replace(pos_n, pos_m - pos_n, "");	
	}		

//////////////////////////////////////	
	// 108
	pos4 = sdp.find("108", pos1);
	if(pos4 != std::string::npos)
	{
		sdp.replace(pos4, 4, ""); // 108+space
	}
	//a=rtpmap:108
	//a=ssrc-group:
	pos_n = sdp.find("a=rtpmap:108", pos1);
	pos_m = sdp.find("a=ssrc-group:", pos1);
	if((pos_n != std::string::npos) && (pos_m != std::string::npos))
	{
		sdp.replace(pos_n, pos_m - pos_n, "");	
	}		

//////////////////////////////////////	
	
	/*
   a=rtpmap:125 red/90000\r\n
   a=rtpmap:107 rtx/90000\r\n
   a=fmtp:107 apt=125\r\n
	*/
	pos4 = sdp.find("125", pos1);
	if(pos4 != std::string::npos)
	{
		sdp.replace(pos4, 4, ""); // 108+space
	}
	pos4 = sdp.find("107", pos1);
	if(pos4 != std::string::npos)
	{
		sdp.replace(pos4, 4, ""); // 108+space
	}
	
	pos_n = sdp.find("a=rtpmap:125", pos1);
	pos_m = sdp.find("a=ssrc-group:", pos1);
	if((pos_n != std::string::npos) && (pos_m != std::string::npos))
	{
		sdp.replace(pos_n, pos_m - pos_n, "");	
	}		
	
////////////////////////////////////////////////////
/*a=extmap:13 urn:3gpp:video-orientation
  a=extmap:3
*/
	pos_n = sdp.find("a=extmap:13", pos1);
	pos_m = sdp.find("a=extmap:3", pos1);
	if((pos_n != std::string::npos) && (pos_m != std::string::npos))
	{
		sdp.replace(pos_n, pos_m - pos_n, "");	
	}		
/*
a=extmap:4 urn:ietf:params:rtp-hdrext:sdes:mid
a=extmap:5 
*/	
	pos_n = sdp.find("a=extmap:4", pos1);
	pos_m = sdp.find("a=extmap:5", pos1);
	if((pos_n != std::string::npos) && (pos_m != std::string::npos))
	{
		sdp.replace(pos_n, pos_m - pos_n, "");	
	}		
	
	//printf("send sdp to peer:++++++++++++++\n%s \n", sdp.c_str());
	
	return sdp;

#endif 	
	
#if 0
	std::string sdp_modi;
	
	std::string sdp_head_part;	
	std::string sdp_mid_part;
	std::string sdp_last_part;	
	std::string video_modi;
	std::string rtcp_fb;
	
	std::string m_video;
	std::string line_split="\r\n";
	//size_t pos;
	
	size_t pos1 = sdp.find("m=video");
	if (pos1 == std::string::npos) 
	{
		return "";
	}
	
	// head
	sdp_head_part = sdp.substr(0, pos1); 
	
	//m=video 9 UDP/TLS/RTP/SAVPF 98 99 96 97 100 101 102 123 127 122 125
	// video
	m_video     = sdp.substr(pos1, 128); 
	size_t pos2 = m_video.find("98");
	if(pos2 != std::string::npos)
	{
		video_modi = m_video.substr(0, pos2-1); 
		
		size_t pos3 = video_modi.find("96");
		size_t pos4 = video_modi.find("97");
		if((pos3 != std::string::npos) && (pos4 != std::string::npos))
		{
			video_modi.replace(pos3, 2, "98");
			video_modi.replace(pos4, 2, "99");
		}
		
		video_modi = video_modi + line_split;
	}
	
	// c=IN IP4
	pos1 = sdp.find("c=IN IP4", pos1);
	pos2 = sdp.find("a=rtpmap:96", pos1);	
	sdp_mid_part = sdp.substr(pos1, pos2 - pos1); 
	
	//a=rtpmap-fb:98 
	while(1)
	{
		size_t pos_n = sdp.find("a=rtpmap:98", pos2);
		if(pos_n == std::string::npos)
		{
			break;
		}
		size_t pos_m = sdp.find("a=rtpmap:100", pos2);
		if(pos_m == std::string::npos)
		{
			break;
		}		
		rtcp_fb = sdp.substr(pos_n, pos_m - pos_n); 
		break;
	}
		
	//a=ssrc-group:
	pos1 = sdp.find("a=ssrc-group", pos2);
	if(pos1 != std::string::npos)	
	{
		sdp_last_part = sdp.substr(pos1, -1); 
	}
	
	//sdp_modi = sdp_head_part + sdp_mid_part + video_modi + rtcp_fb + sdp_last_part;
	sdp_modi = sdp_head_part + sdp_mid_part + video_modi + rtcp_fb;
	
	printf("send sdp to peer:++++++++++++++\n%s \n", sdp_modi.c_str());
	
	return sdp_modi;

#endif 
	
#if 1
  // change Video codec order to test openH264 decoder. openH264 decoder has not output a frame. 
		// But VP8  is ok.
		size_t pos = sdp.find("m=video");
		if (pos != std::string::npos) 
		{
			std::string video_str = sdp.substr(pos, 128); 
			
			//m=video 9 UDP/TLS/RTP/SAVPF 98 99 96 97 100 101 102 123 127 122 125
			size_t pos2 = video_str.find("98");
			size_t pos1 = video_str.find("96");
			if((pos1 != std::string::npos) && (pos2 != std::string::npos))
			{
				video_str.replace(pos1, 2, "98");
				video_str.replace(pos2, 2, "96");
			}
			
			pos2 = video_str.find("99");
			pos1 = video_str.find("97");
			if((pos1 != std::string::npos) && (pos2 != std::string::npos))
			{
				video_str.replace(pos1, 2, "99");
				video_str.replace(pos2, 2, "97");
			}
			printf("++++++++++++++++++ sdp modify ++++++++++++++  \n");
			sdp.replace(pos, video_str.length(), video_str);
		}
		printf("send sdp to peer:++++++++++++++\n%s \n", sdp.c_str());
		
	return 	sdp;
#endif	
}

std::string forceVideoBandwidth(std::string& sdp, int bitrate_kbps) {
    // 1. 设置目标带宽限制字符串 (b=AS:<kbps>)
    // AS = Application Specific Maximum (单位是 kbps)
    std::string bandwidth_line = "b=AS:" + std::to_string(bitrate_kbps) + "\r\n";

    // 2. 定位 video media section
    size_t video_pos = sdp.find("m=video");
    if (video_pos == std::string::npos) {
        return sdp; // 没有video部分，直接返回
    }

    // 3. 确定 Video Section 的范围
    // 我们只关心当前 video m-line 到下一个 m-line (或文件结束) 之间的区域
    size_t next_m_line = sdp.find("m=", video_pos + 1);
    if (next_m_line == std::string::npos) {
        next_m_line = sdp.length();
    }

    // 4. 检查是否已存在 b=AS，避免冲突或重复添加
    // 注意：只检查 video section 内部
    std::string video_section = sdp.substr(video_pos, next_m_line - video_pos);
    if (video_section.find("b=AS:") != std::string::npos) {
        // 如果已经存在带宽限制，为了确保 30M 生效，这里有两个策略：
        // 策略A (简单): 直接返回，假设已有的值是正确的（这也是你原代码的逻辑）
        // return sdp; 
        
        // 策略B (强硬 - 推荐): 既然我们要"强行"解除限制，最好是找到旧的并替换掉。
        // 但为了保持代码结构和你提供的一致，这里暂时仅做简单的重复检查。
        // 如果想覆盖，需写额外的 replace 逻辑。
        // 这里为了安全起见，如果发现已存在，我们可以不做操作，或者简单地在头部再次插入让 WebRTC 读取第一个。
        // 绝大多数 SDP 解析器会采用最后读取到的值或者第一个值。
        // 为了简单，如果没找到才插入：
    }

    // 5. 找到 m=video 行的结束位置
    size_t video_line_end = sdp.find("\r\n", video_pos);
    if (video_line_end == std::string::npos) {
        return sdp;
    }

    // 6. 将 b=AS 配置插入到 video section 的最顶端 (紧跟 m=video 行之后)
    // 根据 RFC 4566，b= 行应该放在 c= 行之后，a= 行之前。
    // 但在 WebRTC 实现中，把它放在 m= 行紧随其后通常也是生效的，且优先级较高。
    size_t insert_pos = video_line_end + 2; // +2 跳过 \r\n
    
    // 如果紧接着 m=video 这一行下面有 c=IN IP4 ... 这样的连接行，建议插入到 c= 行之后
    // 简单的判断逻辑：
    if (sdp.substr(insert_pos, 2) == "c=") {
        size_t c_line_end = sdp.find("\r\n", insert_pos);
        if (c_line_end != std::string::npos) {
            insert_pos = c_line_end + 2;
        }
    }

    sdp.insert(insert_pos, bandwidth_line);

    return sdp;
}

std::string RemoveTransportCC(const std::string& sdp) {
    std::stringstream ss(sdp);
    std::string line;
    std::string new_sdp;

    while (std::getline(ss, line)) {
        // 1. 过滤掉 transport-cc 的扩展头定义
        if (line.find("transport-wide-cc-extensions") != std::string::npos) {
            continue; 
        }
        // 2. 过滤掉 transport-cc 的反馈机制
        if (line.find("transport-cc") != std::string::npos) {
            continue;
        }
        
        // 这里的换行符取决于你的 SDP 格式，通常是 \r\n
        new_sdp += line + "\r\n";
    }
    return new_sdp;
}