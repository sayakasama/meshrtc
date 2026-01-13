#include <string>

#include "cjson.h"
#include "json_codec.h"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"
#include "mesh_rtc_call.h"

#include "cfg.h"
#include "av_api.h"
#include "av_cfg.h"
#include "av_conn.h"

#ifndef GB28181
#include "video_data_recv.h"
#endif
/////////////////////////////////////////////////////////////////////
extern av_params_t    av_params;
extern av_audio_cfg_t av_audio_cfg;
extern char av_peer_ip[32];
extern int  rpc_port;

static int  av_module_status  = 1;
static int  av_module_lost    = 2;

const char *AV_MODULE_FONTS_FILE = "/data/local/s10/version/fonts.tar.gz";
//////////////////////////////////////////////////////////////////////////////////////
void av_sync_time();
void av_fonts_dl(int  dev_type, const char *fonts_file);
//////////////////////////////////////////////////////////////////////////////////////
int is_date_changed()
{	
	static u_int64_t sys_time_last = 0; // seconds
	static float     up_time_last  = 0;
	u_int64_t sys_time_now  = 0; // seconds
	float     up_time_now, tmp;
	u_int64_t diff1, diff2;
	int  ret;
	char buf[128];
	
	FILE *fp = fopen("/proc/uptime", "r");
	if(!fp)
	{
		return 0;
	}
	
	memset(buf, 0, 128);
	ret = fread(buf, 1, 128, fp);
	fclose(fp);
	if(ret <= 0)
	{
		return 0;
	}
	
	ret = sscanf(buf, "%f %f", &up_time_now, &tmp);
	if(2 != ret)
	{
		return 0;
	}
	
	if(0 == sys_time_last)
	{
		// init uptime & sys_time
		up_time_last  = up_time_now;
		sys_time_last = sys_time_get()/1000;
		return 0;
	}
		
	sys_time_now  = sys_time_get()/1000;		
	diff1 = sys_time_now - sys_time_last;
	diff2 = up_time_now  - up_time_last;
	ret   = diff2 - diff1; // 32 bit int is enough
	if((ret < 2) && (ret > -2))
	{
		return 0; // If diff less 2 seconds, assume it not changed
	}
	
	sys_time_last = sys_time_now;
	up_time_last  = up_time_now;

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "date changed? from %lu to %lu ", sys_time_last, sys_time_now); 

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////
void av_audio_encoder_restart()
{
	static u_int64_t time = 0;
	u_int64_t now;
	char cmd[256];
	
	if(0 == time)
	{
		time = sys_time_get();
		return;
	}
	
	now = sys_time_get();
	if((now - time) < 10 * 1000)
	{
		return;
	}
	time  = now;
	
	strcpy(cmd, "stop audio_encode");
	android_system(cmd);
	sleep_ms(100); // wait for stop 
	
	strcpy(cmd, "start audio_encode");
	android_system(cmd);
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " restart audio_encoder.............. ");
}
//////////////////////////////////////////////////////////////////////////////////////
void av_module_conn_check(int dev_type)
{
	unsigned int expect_sn;
	unsigned int sn;	
	int  ret;
	char cmd_req[32];
	char cmd_rsp[32];
	static unsigned int hello_sn = 0;
	
	strcpy(cmd_req, "hello");

	ret = hello(cmd_req, cmd_rsp);
	if (ret != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " hello resp msg is not received ");
		av_server_status_update(dev_type, 0); // hello req failed
		return;
	}
	
	if (!strstr(cmd_rsp, "ok"))    // hello msg format error 
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " hello resp msg is not correct ");
		av_server_status_update(dev_type, 0);
		return;
	}
	
	ret = sscanf(cmd_rsp, "ok %d", (int *)&sn);
	if (ret != 1)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " hello resp format is not correct ");
		av_server_status_update(dev_type, 0); // hello msg format error
		return;
	}

	if (hello_sn == 0)
	{
		// first hello and setup conn ok. 
		// Notice: we have update cfg to av module in startup stage
		hello_sn = sn;
		av_server_status_update(dev_type, 1); 
		return;
	}
	
	expect_sn = hello_sn + 1;
	hello_sn  = sn;	
	if (expect_sn != sn)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " peer sn is not correct in resp ");
		av_server_status_update(dev_type, 0); // peer restart ?
		return;
	}

	av_server_status_update(dev_type, 1);
	
	if(is_date_changed())
	{
		av_sync_time();
	}
}
/////////////////////////////////////////////////////////////////////
void av_sync_time()
{
#ifdef LINUX_X86
	// do nothing. self test 
#else		
	char desValue[64] = {0};
	char cmd[256];
	const char *tmp_file    = "/tmp/sync_time.txt";
    const char *date_format = "+%Y.%m.%d-%H:%M:%S";	
	int ret;
	char rpc_addr[128];	
	
	sprintf(rpc_addr, "http://%s:%d", av_peer_ip, rpc_port);
	
	sprintf(cmd, "date \"%s\" > %s", date_format, tmp_file);
	android_system(cmd);
	FILE *fp = fopen(tmp_file, "r");
	if(!fp)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get local data failed ");
	    return ;
	}

	ret = fread(desValue, 1, 63, fp);
	fclose(fp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " no local date info get ?");
		return;
	}
	
	sprintf(cmd, "av_cmd %s date -s %s ", rpc_addr, desValue);
	android_system(cmd);

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " sync time to av module : %s", cmd);
	
	return ;	
#endif
}
/////////////////////////////////////////////////////////////////////////////////////
void  av_hw_status_get()
{
	char cmd[256];
	char av_module_addr[128];	
	
	sprintf(av_module_addr, "http://%s:%d", av_peer_ip, rpc_port);
	
	sprintf(cmd, "av_cmd %s cat %s > %s", av_module_addr, AV_HDMI_STATUS_FILE, AV_HDMI_STATUS_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif
	
	sprintf(cmd, "av_cmd %s cat %s > %s", av_module_addr, AV_PTT_STATUS_FILE, AV_PTT_STATUS_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif
	
	sprintf(cmd, "av_cmd %s cat %s > %s", av_module_addr, AV_AUDIO_EAR_STATUS_FILE, AV_AUDIO_EAR_STATUS_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif
	
	sprintf(cmd, "av_cmd %s cat %s > %s", av_module_addr, AV_HDMI_RES_FILE, AV_HDMI_RES_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "sync av hw status to local /tmp/ on av connected status");
}
/////////////////////////////////////////////////////////////////////////////////////
void  av_hw_status_reset()
{
    char status[64];
	char cmd[256];

    sprintf(status, AV_HDMI_STATUS_FORMAT, 0);	
	sprintf(cmd, "echo  %s > %s", status, AV_HDMI_STATUS_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif

    sprintf(status, AV_PTT_STATUS_FORMAT, 0);	
	sprintf(cmd, "echo  %s > %s", status, AV_PTT_STATUS_FILE);	
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif
	
    sprintf(status, AV_AUDIO_STATUS_FORMAT, 0);	
	sprintf(cmd, "echo  %s > %s", status, AV_AUDIO_EAR_STATUS_FILE);		
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif
	
    sprintf(status, AV_HDMI_RES_FORMAT, 0, 0, 0);	
	sprintf(cmd, "echo  %s > %s", status, AV_HDMI_RES_FILE);
#ifdef LINUX_X86
	system(cmd);
#else	
	android_system(cmd);
#endif

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "reset av hw status on av disconn");
}
/////////////////////////////////////////////////////////////////////
int av_peerip_update(int  dev_type)
{
	static int reboot_count = 0;
	int   ret;
	char  cmd[256], *p;	
	const char *ip_file_name   = "/tmp/local_ip_addr";
	char  ip_in_av_module[128] = {0};
	unsigned char local_ip[32] = {0};
	char rpc_addr[128];	
	
	sprintf(rpc_addr, "http://%s:%d", av_peer_ip, rpc_port);
	
	av_internal_ip_get(dev_type, local_ip);
	
	sprintf(cmd, "av_cmd %s /customer/app/get_env.sh peerIp > %s ", rpc_addr, ip_file_name);	
#ifdef LINUX_X86
	system(cmd);
#else		
	android_system(cmd);
#endif

	FILE *fp = fopen(ip_file_name, "r");
	if(!fp)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get peer ip failed on av module ");
	    return 0;
	}

	memset(ip_in_av_module, 0, 128);
	ret = fread(ip_in_av_module, 1, 127, fp);
	fclose(fp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " no peer ip info on av module  ?");
		return 0;
	}
	
	//delete  \n in the tail
	p = ip_in_av_module;
	while(*p != 0)
	{
		p++;
		if( (*p != '.') && 
		  ((*p < '0') || (*p > '9')) )
		{
			*p = 0;
			break;
		}
	}

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " peer ip in av_module : %s", ip_in_av_module);
	if(0 == strcmp(ip_in_av_module, (char *)local_ip))
	{
	    return 0;
	}

	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " ip_in_av_module %s is not same as local_ip=%s , reset it and reboot av module(reboot_count=%d)", ip_in_av_module, (char *)local_ip, ++reboot_count);
	if(reboot_count > 2)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_ERR, "++++++ set the av_peer_ip of av module and reboot_count > 2, av module is wrong ??? ++++++ \n"); 
	}
	
	sprintf(cmd, "av_cmd %s /customer/app/set_env.sh peerIp %s ", rpc_addr, local_ip);
#ifdef LINUX_X86
	system(cmd);
#else		
	android_system(cmd);
#endif
	
	sprintf(cmd, "av_cmd %s reboot ", rpc_addr);
#ifdef LINUX_X86
	system(cmd);
#else		
	android_system(cmd);
#endif
	
	return 1;	
}
/////////////////////////////////////////////////////////////////////
void av_module_cfg_update()
{	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " av_module_cfg_update: update video & audio config data ");
	
	// for debug & test
	av_audio_cfg_dump(&av_audio_cfg);
	av_video_cfg_dump(&av_params);
	
	// update video params
	video_cfg_set((char *)&av_params, sizeof(av_params));
		
	// update audio params
	audio_cfg_set((char *)&av_audio_cfg, sizeof(av_audio_cfg));		
}
/////////////////////////////////////////////////////////////////////
void av_server_status_update(int  dev_type, int status)
{
	char buf[32];
	int ret;
	
	if(1 == status)
	{
		if(0 == av_module_status)
		{
			// 1) On base station such as 9300M etc, c400 ip addr is dynamic config
			// 2) In some case, S10 av module was plugged out of 9300M device and installed in Z800A/B devices,
			// the ip addr in S10 was not correct
			{
#ifdef LINUX_X86
				// selft test on linux x86 and no set peer ip
#else				
				ret = av_peerip_update(dev_type);
				if(ret == 1)
				{
					// wait av module re-connected
					return;
				}
#endif				
			}
			
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main] av_module is connected !! \n");
			// re-conn av_module and update cfg
			av_module_cfg_update();
			//sync time
			av_sync_time();
			//sync status
			av_hw_status_get();
			// dl fonts
			av_fonts_dl(dev_type, AV_MODULE_FONTS_FILE);
			
			sprintf(buf, "echo %d > %s", status, AV_SERVER_STATUS_FILE);
#ifdef LINUX_X86
			system(buf);
#else			
			android_system(buf);		
#endif		
		}
		av_module_status = 1;
		av_module_lost   = 0;
		
	} else {
		
		av_module_lost++;
		if(av_module_lost > 1)
		{
			if(1 == av_module_status)
			{
				MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "[rtc_main] warning ===> av_module is disconn !! \n");
				sprintf(buf, "echo %d > %s", status, AV_SERVER_STATUS_FILE);
#ifdef LINUX_X86
				system(buf);
#else			
				android_system(buf);		
#endif	
				av_hw_status_reset();
			}
			av_module_status = 0;
		}
	}	
}
/////////////////////////////////////////////////////////////////////
void mesh_video_res_set(int scale_down)
{
#ifndef GB28181
	av_params_t     video_cfg_out;
	
	if(scale_down)
	{
		memcpy((void *)&video_cfg_out, (void *)&av_params, sizeof(av_params));
		
		video_cfg_out.video_cfg.w = screen_width_max;
		video_cfg_out.video_cfg.h = screen_height_max;
		
		video_cfg_set((char *)&video_cfg_out, sizeof(av_params));
		encoder_wh_set(video_cfg_out.video_cfg.w, video_cfg_out.video_cfg.h);
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " scale down video res to w=%d h=%d ", video_cfg_out.video_cfg.w, video_cfg_out.video_cfg.h);
	} else {
		video_cfg_set((char *)&av_params, sizeof(av_params));
		encoder_wh_set(av_params.video_cfg.w, av_params.video_cfg.h);
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " restore video w=%d h=%d to config value", av_params.video_cfg.w, av_params.video_cfg.h);
	}
#endif	
}
/////////////////////////////////////////////////////////////////////
void av_fonts_dl(int  dev_type, const char *fonts_file)
{
    struct stat st;
	char cmd[256];
	char rpc_addr[128];	
	unsigned char local_ip[32] = {0};
	unsigned char mac[32]      = {0};
	
#ifdef LINUX_X86
	return; // selft test and not sync to av module
#endif

	if((dev_type != MESH_DEV_T_Z800BG) && (dev_type != MESH_DEV_T_Z800AG))
	{
		return; // not GBCOM AV module, no video service
	}

    if (stat(fonts_file, &st) == -1) 
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "fonts file %s not exist?", fonts_file);
        return;
	}

	// get local ip addr
	local_ip_mac_get(MESH_TERMINAL_INTERNAL_IF, local_ip, mac);
	if(0 == strlen((const char*)local_ip))
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get local ip failed on %s for fonts downloading? ", MESH_TERMINAL_INTERNAL_IF);
		return ;
	}
		
	sprintf(rpc_addr, "http://%s:%d", av_peer_ip, rpc_port);
	
	sprintf(cmd, "av_cmd %s /customer/app/get_fonts.sh %s", rpc_addr, local_ip);
	android_system(cmd);
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, "fonts file %s downloaded to av module ", fonts_file);
}	
//////////////////////////////////////////////////////////////////////////////////
void av_audio_cfg_dump(av_audio_cfg_t *audio_cfg)
{
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "enable=%d samples=%d aec=%d volume=%d", audio_cfg->enable, audio_cfg->samples, audio_cfg->aec, audio_cfg->volume);
}
//////////////////////////////////////////////////////////////////////////////////
void av_video_cfg_dump(av_params_t *av_params)
{
	av_osd_cfg_t   *osd       = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;

	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "codec=%d gop=%d qp=%d fps=%d code_rate=%d w=%d h=%d", \
					video_cfg->codec, video_cfg->gop, video_cfg->qp, video_cfg->fps, video_cfg->code_rate, video_cfg->w, video_cfg->h); 	
						
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "time_show=%d, time_format=%s, time_position=%d, time_text_size=%d, time_margin=%d", osd->time_show, osd->time_format, osd->time_position, osd->time_text_size, osd->time_margin);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "time_offset_x=%d, time_offset_y=%d", osd->time_offset_x, osd->time_offset_y);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "title_show=%d, title_position=%d, title_text_size=%d, title_margin=%d, text_len=%d \n, title_text=%s", osd->title_show, osd->title_position, osd->title_text_size, osd->title_margin, osd->text_len, osd->title_text);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "text_offset_x=%d, text_offset_y=%d", osd->text_offset_x, osd->text_offset_y);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "gnss_show=%d, gnss_position=%d, gnss_text_size=%d, gnss_margin=%d", osd->gnss_show, osd->gnss_position, osd->gnss_text_size, osd->gnss_margin);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "gnss_offset_x=%d, gnss_offset_y=%d", osd->gnss_offset_x, osd->gnss_offset_y);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "bat_sig_show=%d, bat_sig_position=%d, bat_sig_text_size=%d, bat_sig_margin=%d", osd->bat_sig_show, osd->bat_sig_position, osd->bat_sig_text_size, osd->bat_sig_margin);
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "bat_sig_offset_x=%d, bat_sig_offset_y=%d", osd->bat_sig_offset_x, osd->bat_sig_offset_y);
}
//////////////////////////////////////////////////////////////////////////////////
int av_internal_ip_get(int  dev_type, unsigned char *local_ip)
{
	unsigned char mac[32] = {0};
	char  interface[32]   = {0};
	
	*local_ip = 0;
	
#ifdef LINUX_X86
	strcpy(interface, "enp0s3"); // To use network port on my virtual machine. we config ip addr 169.167.x.x on enp0s3:2 port
#else
	if(dev_type == MESH_DEV_T_9300M)
	{
		//基站系列产品网络规划：
		//br0: 172.18.0.0; br0:1 169.167.0.0; av_module: 169.167.123.113
		strcpy(interface, MESH_BTS_INTERNAL_IF);
	} else {
		//Z800A/B, XM2300, etc网络规划：
		//br0:172.18.0.0(XM模组可能没有ip地址)，br0:1 169.167.123.112 Z800A/B av_module:169.167.123.113
		//XM2300无音视频板卡
		strcpy(interface, MESH_TERMINAL_INTERNAL_IF);
	}	
#endif

	while(1)
	{
		local_ip_mac_get(interface, local_ip, mac);
		if(strlen((const char*)local_ip))
		{
			break;
		}
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get local ip failed on %s ", interface);
		sleep_ms(1000);
	}
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get local ip %s on %s ", local_ip, interface);
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_external_ip_get(int  dev_type, char *ip)
{
	char interface[32]    = {0};
	unsigned char mac[32] = {0};
	
#ifdef LINUX_X86
	strcpy(interface, "enp0s3"); // To use network port on my virtual machine. 
#else	
	if(dev_type == MESH_DEV_T_9300M)
	{
		//诸如9300M等基站设备虽然运行了RTC_CLIENT，但不对外推流，后续可能删除br0:1端口
		//因此绑定到br0接口
		strcpy(interface, MESH_TERMINAL_EXTERNAL_IF);
	}
	else if (dev_type == MESH_DEV_T_XM2300)
	{
		//DJI模块使用可能br0没有ip地址，优先搜索br0
		//若找不到ip地址，则使用eth0
		strcpy(interface, MESH_TERMINAL_EXTERNAL_IF);
		local_ip_mac_get(interface, (unsigned char*)ip, mac);
		if(strlen((const char*)ip))
		{
			MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get external ip %s on %s ", ip, interface);
			return 1;
		}
		strcpy(interface, MESH_MAINSTAT_EXTERNAL_IF);
	}
	else
	{
		strcpy(interface, MESH_TERMINAL_EXTERNAL_IF);
	}		
#endif

	while(1)
	{
		local_ip_mac_get(interface, (unsigned char*)ip, mac);	
		if(strlen((const char*)ip))
		{
			break;
		}
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get external ip failed on %s ", interface);
		sleep_ms(1000);
	}	
	
	MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get external ip %s on %s ", ip, interface);	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_video_conn_ok()
{
	int ret, status;
	char hdmi_status[64];
	
	// Not scan hdmi status file while av module is not connected
	if(0 == av_module_status)
	{
		return 0;
	}
	
	FILE *fp = fopen(AV_HDMI_STATUS_FILE, "r");
	if(!fp)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " get hdmi status file %s failed  ", AV_HDMI_STATUS_FILE);
	    return 0;
	}

	memset(hdmi_status, 0, 64);
	ret = fread(hdmi_status, 1, 63, fp);
	fclose(fp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " no hdmi_status info in %s ?", AV_HDMI_STATUS_FILE);
		return 0;
	}

	ret = sscanf(hdmi_status, AV_HDMI_STATUS_FORMAT, &status);	
	if(ret != 1)
	{
		MESH_LOG_PRINT(MAIN_MODULE_NAME, LOG_INFO, " failed to get hdmi_status info in %s ", AV_HDMI_STATUS_FILE);
		return 0;
	}
	
	return status;
}