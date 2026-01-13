/*
 */
#ifndef MAIN_WND_USING

#include "absl/flags/parse.h"
#include "api/scoped_refptr.h"

#include "rtc_base/physical_socket_server.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/field_trial.h"
#include "test/field_trial.h"

#include "conductor.h"
#include "conn_ctrl.h"
#include "peer_connection_mesh.h"
#include "peer_connection_client.h"
#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"
#include "ipc_msg.h"

#include "web_passwd.h"
#include "cfg.h"
#include "av_api.h"
#include "av_cfg.h"
#include "av_conn.h"
#include "cfg_28181.h"
#include "video/video_data_recv.h"

#include "comm/http/http_client.h"
#include "comm/websocket/websocket_client.h"

#include "../../av_module/av_common.h"

/////////////////////////FOR GB28181/////////////////////////////
#define IPC_MSG_CFG_28181           "cfg_28181:"
/////////////////////////////////////////////////////////////////

using namespace std;
/////////////////////////////////////////////////////////////////////
extern const uint16_t kDefaultServerPort;

extern av_params_t    av_params;
extern av_audio_cfg_t av_audio_cfg;

extern gb_28181_cfg_t gb_28181_cfg;

extern int g_wh_limited;

static int  pcm_sampling_rate = 16000;

static int  ipc_msg_queue_id  = -1;
static int  dev_type          = MESH_DEV_T_Z800BG;

static char av_external_ip[32];
static char av_local_ip[32];
char av_peer_ip[32];
int  rpc_port                 = GSOAP_DEFAULT_PORT;

static int  running           = 1;

int g_rcmode = 0;
/////////////////////////////////////////////////////////////////////
void cfg_video_proc(char *data);
void cfg_audio_proc(char *data);
void cfg_28181_proc(char *data);

void hp_video_start();
void hp_video_stop();

void restore_prev_stat();
void login_remote_rtc_server(const char *rtc_server);
void login_local_rtc_server();
void rtc_test();

void mesh_name_get(char *mesh_name);

void gb28181_client_restart(int gb28181_enable);

extern void mesh_video_res_set(int scale_down);
/////////////////////////////////////////////////////////////////////
void run_in_rtc_thread(ConnCtrl  *ctrl, PeerConnectionClient *client)
{
	if(ctrl)
		ctrl->Process();
	if(client)
		client->Process();
}
/////////////////////////////////////////////////////////////////////
class CustomSocketServer : public webrtc::PhysicalSocketServer {
public:
	explicit CustomSocketServer(ConnCtrl* wnd, PeerConnectionClient *client_c)
		: message_queue_(NULL), wnd_(wnd), conductor_(NULL), client_(NULL), client_c_(client_c) {}
	virtual ~CustomSocketServer() {}

	void SetMessageQueue(webrtc::Thread* queue) override { message_queue_ = queue; }

	void set_client(PeerConnectionMesh* mesh_client) { client_ = mesh_client; }
	void set_conductor(Conductor* conductor) { conductor_ = conductor; }

	// Override so that we can also pump the GTK message loop.
	bool Wait(int cms, bool process_io) override 
	{
		sleep_ms(10);	 // wait for cpu schedule

		if(process_io) // use this flag to call a func once. It's important to avoid re-entrance problem
		{
			if(wnd_)
			{
				//MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "MESH CLIENT Process start!!!!");
				wnd_->Process();
			}
			if(client_c_)
			{
				//MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "RTC CLIENT Process start!!!!");
				client_c_->Process();
			}
		}
		
		if (!wnd_->IsWindow() && !conductor_->connection_active() &&
			client_ != NULL && !client_->is_connected()) 
		{
			message_queue_->Quit();
		}
		
		return webrtc::PhysicalSocketServer::Wait(0 /*cms == -1 ? 1 : cms*/,	process_io);
	}

protected:
	webrtc::Thread* message_queue_;
	ConnCtrl* wnd_;
	Conductor* conductor_;
	PeerConnectionMesh* client_;
	PeerConnectionClient *client_c_;
};
///////////////////////////////////////////////////////////////////
void signal_handler(int sig)
{
	//release socket & memory etc...
	// Notice: Do not handle any blocking operation!!!	
	running = 0;
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " recv signal %d and exit ", sig);
	
	exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void rtc_version()
{
	const char *meshrtc_version = "1.5.7.7";
	printf("meshrtc version : %s \n", meshrtc_version);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
void usage()
{
	printf(" -r 1.2.3.4 : av module ip \n ");
	printf(" -a 0/1 : 1, audio only ; 0, video/audio\n ");
	printf(" -n test_100 : client name \n ");	
	printf(" -p 4567 : rpc port \n ");
	printf(" -v  : meshrtc version info \n ");	
	printf(" -t  : rtc test \n ");

	printf("  \n ");		
}
///////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	bool audio_only = false;
	int opt, count, ret, tmp_rcmode;
	char data[IPC_MSG_MAX_SIZE]; // ipc msg 
	int  size;
	char name[32], passwd[32], mesh_name[32];
	char dev_sn[24], video_id[32];
	int  video_enable, hp_video_enable=0;
	int  audio_encoder_register = 0;
	int  video_set_by_28181_flag = 0;
	
	if((argc > 1) && !strcmp(argv[1], "-v"))
	{
		rtc_version();
		return 0;
	}
	
	sleep(1); // to avoid too fast restart on some rare situation
	
	if(!singleton_lock_get(MAIN_MODULE_NAME))
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "module has a process in running...exit now \n");
		mesh_log_close();
		return -1;
	}
	
	memset(data, 0, IPC_MSG_MAX_SIZE);
	sys_date_get(data);	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO,"\n++++++++++++++++++ %s rtc_client module start running...(compiled on %s %s) ++++++++++++++++++ ", data, __DATE__, __TIME__);
			
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	//signal(SIGSEGV, signal_handler);
	signal(SIGUSR1, signal_handler);

	// get config params
    av_video_cfg_get(&av_params);
    av_audio_cfg_get(&av_audio_cfg);

	// get gb28181 config params
	gb_28181_cfg_get(&gb_28181_cfg);
	// save cfg to video encoder
	video_osd_cfg_update(&av_params.osd_cfg);
	
	pcm_sampling_rate = av_audio_cfg.samples;
	
	// get device specific param
	cfg_dev_sn_get(dev_sn, 24); 
	cfg_dev_type_get(&dev_type);

#ifdef LINUX_X86
	dev_type = MESH_DEV_T_9300M; // for test
#endif

#ifdef MESH_DJI
	dev_type = MESH_DEV_T_XM2300;
#endif

	av_internal_ip_get(dev_type, (unsigned char *)av_local_ip);

	cfg_video_id_get(video_id, 32);
	cfg_video_enable_get(&video_enable);
#ifdef MESH_DJI
	video_enable = 1; // DJI模块需要默认视频开启
#endif
	
	memset(passwd, 0, 32);	
	memset(name, 0, 32);
	memset(mesh_name, 0, 32);	
#ifdef MESH_DJI
	sprintf(name , "%sDJ", dev_sn);
#else
	sprintf(name , "MT_%s", dev_sn);
#endif
	mesh_name_get(mesh_name);
		
#ifdef LINUX_X86
	strcpy(av_peer_ip, "127.0.0.1");
#elif defined(MESH_DJI)
	strcpy(av_peer_ip, "127.0.0.1");
#else
	// if(dev_type == MESH_DEV_T_Z800BH)
	// {
	// 	strcpy(av_peer_ip, "127.0.0.1");
	// 	audio_only = true; // not support video now.
 	// } else {
	strcpy(av_peer_ip, AV_MODULE_DEFAULT_ADDR);
// }
#endif
	
	while ((opt = getopt(argc, argv, "a:f:r:p:n:s:x:ht")) != -1)
	{
		switch (opt) {
		case 'r':
			strcpy(av_peer_ip, optarg); 
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "av_peer_ip=%s \n", av_peer_ip);
		break;
		
		case 'n':
			strcpy(name, optarg); 
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "name=%s \n", name);
		break;
		
		case 't':
			rtc_test();
		break;
	
		case 's':
			pcm_sampling_rate = atoi(optarg); 
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO,"pcm_sampling_rate=%d \n", pcm_sampling_rate);
		break;

		case 'a':
			audio_only = atoi(optarg) > 0;
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO,"audio_only=%d \n", audio_only);
		break;

		case 'x':
			hp_video_enable = atoi(optarg);
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO,"hp_video_enable=%d \n", hp_video_enable);
		break;

		case 'p':
			rpc_port = atoi(optarg);
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO,"rpc_port=%d \n", rpc_port);
		break;
		
		case 'h':
		default: /* '?' */
			usage();
		break;
		}
	}

	if(dev_type != MESH_DEV_T_Z800BH && dev_type != MESH_DEV_T_XM2300) {
		av_server_status_update(dev_type, 0);
	}
	
	audio_samples_set(pcm_sampling_rate);

	if(!video_enable)
	{
		audio_only      = true; 
		hp_video_enable = 0;
	}
	if(hp_video_enable)
	{
		audio_only      = false; 
	}
	
	// ref: system_wrappers/source/field_trial.cc . trials format a/b/c/d/
	// flexFEC on. 
#if 0	//ZLMedia server doest not support fec. so we enable this feature on mesh call and disable on rtc call?
	constexpr char kVideoFlexfecFieldTrial[] = "WebRTC-FooFeature/Enabled/WebRTC-FlexFEC-03-Advertised/Enabled/";
	webwebrtc::field_trial::InitFieldTrialsFromString(kVideoFlexfecFieldTrial);
#endif
	 //shangtao TODO
#ifndef MESH_DJI
 	constexpr char kVideoFlexfecFieldTrial[] = "WebRTC-FooFeature/Enabled/WebRTC-Video-BalancedDegradation/Enabled/WebRTC-FrameDropper/Disabled/";
	webwebrtc::field_trial::InitFieldTrialsFromString(kVideoFlexfecFieldTrial);
#else
	constexpr char kQualityFirstFieldTrial[] =
    "WebRTC-Video-BalancedDegradation/Disabled/"     // 关闭质量/帧率自适应
    "WebRTC-QualityScaler/Disabled/"                 // 关闭分辨率动态缩放
    "WebRTC-FrameDropper/Disabled/";           // 禁止丢帧
	webwebrtc::field_trial::InitFieldTrialsFromString(kQualityFirstFieldTrial);
#endif
	//constexpr char kVideoFlexfecFieldTrial[] = "WebRTC-FooFeature/Disabled/WebRTC-Video-BalancedDegradation/Disabled/WebRTC-FrameDropper/Disabled/WebRTC-ImprovedBitrateEstimate/Enabled";
	//constexpr char kVideoFlexfecFieldTrial[] = 
    //"WebRTC-FooFeature/Enabled/"
    //"WebRTC-Video-BalancedDegradation/Enabled,min-fr=25,min-res=1280x720/"
    //"WebRTC-FrameDropper/Disabled/"
	//"WebRTC-ImprovedBitrateEstimate/Enabled";
	//webwebrtc::field_trial::InitFieldTrialsFromString(kVideoFlexfecFieldTrial);

	// default params
	const std::string server = "127.0.0.1";
	
	web_passwd_calc(name, passwd);
	
	ConnCtrl  conn_ctrl(server.c_str(), kDefaultServerPort, name, passwd, video_id, mesh_name);
	PeerConnectionClient webrtc_client(audio_only);	
	
	CustomSocketServer socket_server(&conn_ctrl, &webrtc_client);
	webrtc::AutoSocketServerThread thread(&socket_server);

	webrtc::InitializeSSL();

	PeerConnectionMesh   mesh_client;
	webrtc::scoped_refptr<Conductor> conductor(new webrtc::RefCountedObject<Conductor>(&mesh_client, &webrtc_client, &conn_ctrl, av_peer_ip, av_local_ip, pcm_sampling_rate));
	
    // any meaning?
	socket_server.set_client(&mesh_client);
	socket_server.set_conductor(conductor);

	thread.Start();

	sleep_ms(100);

	char rpc_addr[128];
	
	sprintf(rpc_addr, "http://%s:%d", av_peer_ip, rpc_port);
	av_api_init(rpc_addr);

	// ipc msg queue init
	ipc_msg_queue_id = ipc_msg_thread_start(RTC_CONN_MODULE_NAME, 0);

	if(audio_only)
	{
		conductor->video_enable(false);
	}
	if(hp_video_enable)
	{
		hp_video_stop();
		sleep(1);
		hp_video_start();
	}

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "dev_type=%d dev_sn=%s video_id=%s audio_only=%d mesh_name=%s ", dev_type, dev_sn, video_id, audio_only, mesh_name);
	
	restore_prev_stat();
		
	count = 0;
	while (running)
	{
		sleep_ms(10);

		// get msg and handle from ipc msg queue
		memset(data, 0, IPC_MSG_MAX_SIZE);
		bool waiting = conn_ctrl.AppCmdWaiting(); // prev cmd still in processing queue
		ret          = MESH_RTC_FAIL;
		if(!waiting)
		{
			ret = ipc_msg_get(ipc_msg_queue_id, data, &size, IPC_MSG_MAX_SIZE);
		}
		if(ret == MESH_RTC_OK)
		{
			if(0 == memcmp(data, IPC_MSG_CFG_VIDEO, strlen(IPC_MSG_CFG_VIDEO)))
			{
				// close current conn
				// 配置保存要放在HangUp完成后，否则会引发重拉流配置异常，信令卡死。
				conductor->OnClientHangup();
				sleep_ms(100);
				//提前保存接收到的配置，保证web界面显示与配置一致。
				memcpy(&av_params,data + strlen(IPC_MSG_CFG_VIDEO), sizeof(av_params));
				av_video_cfg_set(&av_params);
				
				cfg_video_proc(data + strlen(IPC_MSG_CFG_VIDEO));
				// notify rtc server params changed.
				webrtc_client.TriggerCfgReport();
			} 
			else if(0 == memcmp(data, IPC_MSG_CFG_AUDIO, strlen(IPC_MSG_CFG_AUDIO)))
			{
				//提前保存接收到的配置，保证web界面显示与配置一致。
				memcpy(&av_audio_cfg, data + strlen(IPC_MSG_CFG_AUDIO), sizeof(av_audio_cfg));
				av_audio_cfg_set(&av_audio_cfg);
				cfg_audio_proc(data + strlen(IPC_MSG_CFG_AUDIO));
				// notify rtc server params changed.
				webrtc_client.TriggerCfgReport();
			}
			else if(0 == memcmp(data, IPC_MSG_CFG_28181, strlen(IPC_MSG_CFG_28181)))
			{
				cfg_28181_proc(data + strlen(IPC_MSG_CFG_28181));
				// 通报国标28181配置变更
				//webrtc_client.TriggerGb28181Report();
			}
			else if(0 == strcmp(data, AUDIO_PROC_REGISTER))
			{
				if(dev_type == MESH_DEV_T_Z800BH || dev_type == MESH_DEV_T_XR9700 || dev_type == MESH_DEV_T_Z900)
				{
					if(audio_encoder_register == 0)
					{
						av_audio_encoder_restart();
						system("stop audio_mixer");
						sleep_ms(100);
						system("start audio_mixer");
						audio_encoder_register = 1;
					}
					ipc_msg_send(AV_AUDIO_MODULE_NAME, (char *)&av_audio_cfg, sizeof(av_audio_cfg));	// forward msg to audio encoder	
					// send current cfg to audio process
					MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "audio process re-connected & update config enable=%d", av_audio_cfg.enable);
				}
			}
			else if(0 == strcmp(data, VIDEO_PROC_REGISTER))
			{
				if(dev_type == MESH_DEV_T_Z800BH || dev_type == MESH_DEV_T_XR9700 || dev_type == MESH_DEV_T_Z900) // In fact no video module send req now on Z800BH. 
				{
					ipc_msg_send(AV_VIDEO_MODULE_NAME, (char *)&av_params, sizeof(av_params));	// forward msg to video encoder	
					// send current cfg to video process
					MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "video process re-connected & update config ");
				}
			}
			else 
			{
				conn_ctrl.AppCmdSet((const char *)data);
			}
		}
#ifndef MESH_DJI
		//下面参数每1s检查一次
		if (count % 100 == 0) // 1s
		{
			if(dev_type != MESH_DEV_T_Z800BH)
			{
				av_module_conn_check(dev_type);
				if(!audio_only)
				{
#ifndef LINUX_X86					
					av_video_conn_ok();
					conductor->local_video_use(false); // always use sl0 video input
#endif	//LINUX_X86				
				}
			}
			else 
			{
				// check audio_encode starting status for Z800BH 
				if(0 == audio_encoder_register)
				{
					av_audio_encoder_restart();	
				}
			}


			if(gb28181_server_conn_check())
			{
				if(g_wh_limited)
				{
					MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main]gb28181 running, not set low res.\n");
					mesh_video_res_set(0);
					g_wh_limited = 0;
					video_set_by_28181_flag = 0;
				}
				if(video_set_by_28181_flag == 0)
				{
					// set video config to high quality once
					av_video_cfg_get(&av_params);
					av_video_cfg_t *video_cfg = &av_params.video_cfg;
					video_cfg->qp	  = 26;
					video_cfg_set((char *)&av_params, sizeof(av_params));
					video_set_by_28181_flag = 1;
				}
			}
		}
#endif // MESH_DJI
		if (count % 200 == 0) // 2s
		{
			ret = cfg_rcmode_get(&tmp_rcmode);
			if((ret != -1) && (g_rcmode != tmp_rcmode))
			{
				g_rcmode = tmp_rcmode;
				MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main]rcmode changed to %d\n", g_rcmode);
			}
		}
		if (count % 1000 == 0) // 10s
		{
			// get memory size used by this process
			FILE* fp = fopen("/proc/self/status", "r");
			char line[128], VmRSS[128], VmSize[128];
			memset(VmRSS, 0, 128);
			memset(VmSize, 0, 128);
			while (fgets(line, 128, fp) != NULL)
			{
				if (strncmp(line, "VmRSS:", 6) == 0)
				{
					strcpy(VmRSS, line);
				}
				else if (strncmp(line, "VmSize:", 6) == 0)
				{
					strcpy(VmSize, line);
				}
			}
			fclose(fp);	
			
			// for debug
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main]conn_ctrl: I am still alive.... %d. %s %s \n", count, VmSize, VmRSS);
#ifdef LINUX_X86 // self test on x86 vm. 
			if(count == 5000 * 100)
			{
				MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main]exit from here for gprof or valgrind test.... %d \n", count);
				break;// add for gprof test
			}
#endif			
		}
		count ++;
	}

	if(hp_video_enable)
	{
		hp_video_stop();
	}
	
	ipc_msg_thread_stop(ipc_msg_queue_id);	
	if(dev_type != MESH_DEV_T_Z800BH) {	
		av_server_status_update(dev_type, 0);
	}
	singleton_lock_free();
	
	mesh_log_close();

	conn_ctrl.Destroy();

	webrtc::CleanupSSL();
		
	return 0;
}
/////////////////////////////////////////////////////////////////////
void cfg_video_proc(char *data)
{
	av_osd_cfg_t   *osd = &av_params.osd_cfg;
	av_video_cfg_t *av_cfg = &av_params.video_cfg;
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " cfg_video_proc: update video config data ");
	
	//update local params
	memcpy(&av_params, data, sizeof(av_params));

	osd->text_len = strlen(osd->title_text);
	av_cfg->qp = 26;
	// 将视频配置通过rpc soap发送到av module:s10. 
	video_cfg_set((char *)&av_params, sizeof(av_params));
	// save cfg to video encoder
	video_osd_cfg_update(osd);

	//保存视频配置到prop配置文件
	// av_video_cfg_set(&av_params);

	//打印当前视频配置
	av_video_cfg_dump(&av_params);		
}
/////////////////////////////////////////////////////////////////////
void cfg_audio_proc(char *data)
{
	av_audio_cfg_t  *cfg = (av_audio_cfg_t  *)data;
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " cfg_audio_proc ");
	
	if(cfg->samples != av_audio_cfg.samples)
	{
		AudioSamplesChange(cfg->samples);
	}
	//update local params
	memcpy(&av_audio_cfg, data, sizeof(av_audio_cfg));
	av_audio_cfg.enable = 1;  // audio always on
	
	// set params to av module. 
	audio_cfg_set((char *)&av_audio_cfg, sizeof(av_audio_cfg));		
	
	// store params to config file. 	
	av_audio_cfg_set(&av_audio_cfg);
	
	av_audio_cfg_dump(&av_audio_cfg);
}
/////////////////////////////////////////////////////////////////////
void cfg_28181_proc(char *data)
{
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " cfg_28181_proc ");
	//update local params
	memcpy(&gb_28181_cfg, data, sizeof(gb_28181_cfg));
	// store params to config file. 	
	gb_28181_cfg_set(&gb_28181_cfg);
	//stop client after cfg
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[cfg_28181_proc]recv cfg changed and restart GB28181...");
	gb28181_client_restart(gb_28181_cfg.gb28181_enable);
}
///////////////////////////////////////////////////////////////////
void gb28181_client_restart(int gb28181_enable)
{
	if(gb28181_enable)
	{
		system("start term_gb_client");
		sleep_ms(500);
		system("start 28181_client");
	}
	else
	{
		system("start term_gb_client");
	}
}
///////////////////////////////////////////////////////////////////
void restore_prev_stat()
{
	int  ret, size, num;
	char rtc_server[64];
	char data[IPC_MSG_MAX_SIZE];
	list<string> msg_list;
	
	memset(rtc_server, 0, 64);
	ret = av_tmp_rtc_server_restore(rtc_server, 64);	
	if(ret < 0)
	{
		login_local_rtc_server(); // in last stat, we logined into local rtc server
		return;
	}
	
	sleep_ms(100); // wait for ipc msg thread starting...
	
	//save ipc msg to local list
	num = 0;
	while(ipc_msg_queue_id != -1)
	{
		memset(data, 0, IPC_MSG_MAX_SIZE);		
		ret = ipc_msg_get(ipc_msg_queue_id, data, &size, IPC_MSG_MAX_SIZE);
		if(ret != MESH_RTC_OK)
		{
			break;
		}
		
		num++;
		msg_list.push_back(string(data));
		//MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO," legacy messaes in queues: %s", data);	
	}

	// restore to prev conn status and then process last 3 msg in queue
	login_remote_rtc_server(rtc_server); 
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO," legacy messaes num=%d in queues", num);	
	// resend ipc msg to queue
	if(ipc_msg_queue_id != -1)
	{		
		for (list<string>::iterator it = msg_list.begin(); it != msg_list.end(); ++it)
		{
			string msg = *it;
			if(num < 4) // indeed, we only reserve last 3 msg in ipc queue
			{
				// too old msg discard and only last two msg handle
				MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO," continue process ipc_msg in queue: %s", msg.c_str());
				ipc_msg_send(RTC_CONN_MODULE_NAME, msg.c_str(), strlen(msg.c_str())+1);
			}
			num--;
		}
	}
}
///////////////////////////////////////////////////////////////////
void login_remote_rtc_server(const char *rtc_server)
{
	char cmd[128];
	
	memset(cmd, 0, 128);
	sprintf(cmd, "%s %s", RTC_SERVER_SET_CMD, rtc_server);
	ipc_msg_send(RTC_CONN_MODULE_NAME, cmd, strlen(cmd)+1);
	
	sprintf(cmd, "%s", RTC_LOGIN_CMD);	
	ipc_msg_send(RTC_CONN_MODULE_NAME, cmd, strlen(cmd)+1);
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO," force rtc_client login in center rtc server: %s", cmd);		
}		
///////////////////////////////////////////////////////////////////
void login_local_rtc_server()
{
	char buf[128];
	
	memset(buf, 0, 128);
	sprintf(buf, "%s %s", MESH_SERVER_SET_CMD, "127.0.0.1");
	ipc_msg_send(RTC_CONN_MODULE_NAME, buf, strlen(buf)+1);
	
	sprintf(buf, "%s", MESH_LOGIN_CMD);	
	ipc_msg_send(RTC_CONN_MODULE_NAME, buf, strlen(buf)+1);
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO," force rtc_client login in local rtc server: %s", buf);		
}
/////////////////////////////////////////////////////////////////////
void mesh_name_get(char *mesh_name)
{
	av_external_ip_get(dev_type, av_external_ip);

	sprintf(mesh_name, "%s@mesh", av_external_ip);
}
/////////////////////////////////////////////////////////////////////
void hp_video_start()
{
	char cmd[256];
	
	// rtsp_hp get video sream data and forward to video_data_recv thread
	sprintf(cmd, "hp_rtsp_transfer &");
	android_system(cmd);
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "start hp_rtsp_transfer");
}
/////////////////////////////////////////////////////////////////////
void hp_video_stop()
{
	char cmd[256];
	
	sprintf(cmd, "pkill -f hp_rtsp_transfer");
	android_system(cmd);
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "stop hp_rtsp_transfer");
}
/////////////////////////////////////////////////////////////////////
void WebsocketConnectTest() 
{
	std::string ws_url;

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[WebsocketConnect] start conn \n");
	// wss://1.2.3.4:5678/websocket/{token}/{loginType}?{videoId=1234}
	ws_url         = "ws://10.30.2.8:8972/websocket/eyJhbGciOiJIUzUxMiJ9.eyJqdGkiOiI3YjAwNDRjYmNhYzI0ZGI5OTBmYjE3OTFkZmQ5ODQ0NyIsInN1YiI6Ik1UXzExMTIyMiIsImlzcyI6InRva2VuLXNlcnZlciIsImlhdCI6MTcxMDIxMTE3NiwiZXhwIjoxNzEwMjk3NTc2LCJyb2xlcyI6Ik9wZXJhdGlvblJvbGUiLCJpZCI6IjdiMDA0NGNiY2FjMjRkYjk5MGZiMTc5MWRmZDk4NDQ3IiwidXNlcm5hbWUiOiJNVF8xMTEyMjIifQ.jZP1PrIkvsoMC2FIUMvj7ZN62wp58Y_Fvy0hKTO37QdbEUa0Z8ET0-lP7Jj6SEwSf-ck1bAJXENLZmmHCog-Hw/3?videoId=888888321";
	websocket_session_thread_start(ws_url.c_str());
	
	sleep(5000);
	
	exit(0);
}
/////////////////////////////////////////////////////////////////////
void rtc_test()
{
	char key[64];	
	char value[128];
	
	WebsocketConnectTest();
	
	// following code is for test and not used in release version
	strcpy(key, "videoId");
	strcpy(value, "123456");
	
	set_property(key, value);	
}

#endif // #ifdef MAIN_WND_USING
