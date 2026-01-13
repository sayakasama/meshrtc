#include "mesh_rtc_inc.h"
#include "utils.h"
#include "mesh_rtc_call.h"
#include "cfg.h"
#include <math.h>


#define MAX_QP_MIN_VALUE   33
////////////////////////////////////////////////////////////////
const char *cfg_ptt_unicast_port_key   = "persist.gb.ptt.unicast_port";
const char *cfg_ptt_multicast_port_key = "persist.gb.ptt.multicast_port";
const char *cfg_ptt_priority_key       = "persist.gb.ptt.priority";

const char *cfg_samples_key = "persist.gb.av.samples";
const char *cfg_aec_key     = "persist.gb.av.aec";
const char *cfg_volume_key  = "persist.gb.av.volume";
const char *cfg_audio_chan_key = "persist.gb.av.chan";

const char *cfg_rcmode_key  = "persist.gb.av.rcmode";
const char *cfg_qp_key      = "persist.gb.av.qp";
const char *cfg_codec_key   = "persist.gb.av.codec";
const char *cfg_gop_key     = "persist.gb.av.gop";
const char *cfg_fps_key     = "persist.gb.av.fps";
const char *cfg_bps_key     = "persist.gb.av.bps";
const char *cfg_res_w_key   = "persist.gb.av.w";
const char *cfg_res_h_key   = "persist.gb.av.h";

const char *cfg_gnss_show_key       = "persist.gb.av.gnssShow";
const char *cfg_gnss_position_key   = "persist.gb.av.gnssPosition";
const char *cfg_gnss_text_size_key  = "persist.gb.av.gnssTextSize";
const char *cfg_gnss_marginX_key    = "persist.gb.av.gnssMarginX";
const char *cfg_gnss_marginY_key    = "persist.gb.av.gnssMarginY";

const char *cfg_time_show_key       = "persist.gb.av.timeShow";
const char *cfg_time_format_key     = "persist.gb.av.timeFormat";
const char *cfg_time_position_key   = "persist.gb.av.timePosition";
const char *cfg_time_text_size_key  = "persist.gb.av.timeTextSize";
const char *cfg_time_marginX_key     = "persist.gb.av.timeMarginX";
const char *cfg_time_marginY_key     = "persist.gb.av.timeMarginY";

const char *cfg_title_show_key      = "persist.gb.av.titleShow";
const char *cfg_title_position_key  = "persist.gb.av.titlePosition";
const char *cfg_title_text_size_key = "persist.gb.av.titleTextSize";
const char *cfg_title_marginX_key   = "persist.gb.av.titleMarginX";
const char *cfg_title_marginY_key   = "persist.gb.av.titleMarginY";
const char *cfg_title_text_key      = "persist.gb.av.titleText";

const char *cfg_bat_sig_show_key      = "persist.gb.av.batSigShow";
const char *cfg_bat_sig_position_key  = "persist.gb.av.batSigPosition";
const char *cfg_bat_sig_text_size_key = "persist.gb.av.batSigTextSize";
const char *cfg_bat_sig_marginX_key   = "persist.gb.av.batSigMarginX";
const char *cfg_bat_sig_marginY_key   = "persist.gb.av.batSigMarginY";

const char *cfg_dev_nickname_key  = "persist.gb.sys.device_nickname";
const char *cfg_dev_name_key      = "persist.gb.sys.device_name";
const char *cfg_video_id_key      = "ro.gb.product.CustomerSN";
const char *cfg_dev_sn_key        = "ro.gb.product.CustomerSN";
const char *cfg_dev_type_key      = "ro.gb.product.sysDeviceType";
const char *cfg_dev_d10x_key      = "run.gb.startup.d10x";
const char *cfg_video_enable_key  = "ro.gb.cap.videoEnable";

const char *cfg_rtc_server_key      = "persist.gb.av.rtcServer";

char tmp_file[128];
////////////////////////////////////////////////////////////////
u_int8_t mesh_dev_type_get(int model_id);
int cfg_dev_model_id_get(int *dev_type);
////////////////////////////////////////////////////////////////
int get_property(const char *key, char * desValue, int len)
{
#ifdef LINUX_X86
	// do nothing
#else		
	static int get_init = 1;
	char cmd[256];
	
	if(get_init)
	{
		pid_t pid = getpid();
		sprintf(tmp_file, "/tmp/property_tmp_%d.txt", pid);
		get_init = 0;		
	}
	
#if 0	
	int ret;
    char scValue[256] = {0};
	sprintf(cmd, "setprop %s ", key);
	ret = popen_cmd(cmd, scValue, 255);
	if(ret == 0)
	{
		snprintf(desValue, len, "%s", scValue);
		return 1;
	}
#else	
	sprintf(cmd, "getprop %s > %s", key, tmp_file);
	android_system(cmd);
	FILE *fp = fopen(tmp_file, "r");
	if(fp)
	{
		int ret = fread(desValue, 1, len, fp);
		if(ret >= 1)
		{
			desValue[ret-1] = 0; // delete last \n
			//printf("read len=%d : str=%s ====\n", ret, desValue);
		}
		fclose(fp);
		return 1;
	}
#endif

#endif	
	return -1;
}
////////////////////////////////////////////////////////////////
int set_property(const char *key, const char * scValue)
{
	if(0 == strlen(scValue))	
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " key=%s value is null", key);
		return 0;
	}
	
#ifdef LINUX_X86
	
	return 1;
	
#else
	
	char cmd[256];
#if 0	
    char buf[256] = {0};	
	int ret;
	sprintf(cmd, "setprop %s %s", key, scValue);
	ret = popen_cmd(cmd, buf, 255);
	android_system(cmd);
	if(ret == 0)
	{
		return 1;
	}
	return -1;	
#else	
	sprintf(cmd, "setprop %s %s", key, scValue);
	android_system(cmd);
	return 1;
#endif	

#endif	
}
////////////////////////////////////////////////////////////////
int cfg_rtc_server_get(char *rtc_server)
{
#ifdef LINUX_X86
	
	strcpy(rtc_server, "127.0.0.1");
	return 1;
	
#else	
	
	int ret;
	ret = get_property(cfg_rtc_server_key, rtc_server, 32);
	if(ret <= 0)
	{
		return 0;
	}
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int samples_check(int samples)
{
	if((samples != 8000) && (samples != 16000) && (samples != 48000) )
	{
		return 0;
	}
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_samples_get(int *samples)
{
#ifdef LINUX_X86
	
	*samples = 16000;
	return 1;
	
#else	
	
	int ret;
	char buf[32];

	*samples = 16000;

	ret = get_property(cfg_samples_key, buf, 32);
	if(ret <= 0)
	{
		*samples = 8000; // use default value
		return 1;
	}
	
	ret = samples_check(atoi(buf));
	if(ret)
	{
		*samples = atoi(buf);
	}

#endif
	
	return 1;
}

////////////////////////////////////////////////////////////////
int cfg_samples_set(int samples)
{
	int ret;
	char buf[32];

	ret = samples_check(samples);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", samples);
	
	ret = set_property(cfg_samples_key, buf);
	
	audio_samples_set(samples); // to trigger other progress restart
	
	return ret;
}
////////////////////////////////////////////////////////////////
int aec_check(int aec)
{
	if((aec != 0) && (aec != 1))
	{
		return 0;
	}
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_aec_get(int *aec)
{
#ifdef LINUX_X86

	*aec = 1;
	return 1;
	
#else	

	int ret;
	char buf[32];

	*aec = 1;	
	ret = get_property(cfg_aec_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	
	*aec = atoi(buf);
	
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_aec_set(int aec)
{
	int ret;
	char buf[32];
	
	ret = aec_check(aec);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", aec);
	
	ret = set_property(cfg_aec_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int volume_check(int volume)
{
	if((volume >= 0) && (volume <= 100))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_volume_get(int *volume)
{
#ifdef LINUX_X86

	*volume = 50; // av module default volume is 50
	return 1;
	
#else	

	int ret;
	char buf[32];

	*volume = 60;	
	ret = get_property(cfg_volume_key, buf, 32);
	if(ret <= 0)
	{
		return  1;
	}
	
	ret = volume_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*volume = atoi(buf);
	
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_volume_set(int volume)
{
	int ret;
	char buf[32];

	ret = volume_check(volume);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", volume);
	
	ret = set_property(cfg_volume_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int cfg_audio_chan_get(char *audio_chan)
{
#ifdef LINUX_X86

	strcpy(audio_chan, AV_AUDIO_CAHNN_HAND_MICROPHONE); //auto hand_microphone ear_microphone ble  
	return 1;
	
#else	

	int ret;

	ret = get_property(cfg_audio_chan_key, audio_chan, 32);
	if(ret <= 0)
	{
		return  -1;
	}	
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_audio_chan_set(const char* audio_chan)
{
	int ret;

	ret = set_property(cfg_audio_chan_key, audio_chan);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int codec_check(int codec)
{
	if((codec != VIDEO_CODEC_H264 ) && (codec != VIDEO_CODEC_H265) && (codec != VIDEO_CODEC_H266))
	{
		return 0;
	}
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_codec_get(int *codec)
{
#ifdef LINUX_X86

	*codec = VIDEO_CODEC_H264; // h264
	return 1;

#else	
	
	int ret;
	char buf[32];

	*codec = VIDEO_CODEC_H264; // h264
	ret = get_property(cfg_codec_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	
	ret = codec_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*codec = atoi(buf);

#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int qp_check(int qp)
{
	if((qp >= 10) &&  (qp <= 100))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_qp_get(int *qp)
{
#ifdef LINUX_X86

	*qp = 60;
	return 1;

#else	

	int ret;
	char buf[32];
	
	*qp = 60;
	ret = get_property(cfg_qp_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}

	ret = qp_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*qp = atoi(buf);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_qp_set(int qp)
{
	int ret;
	char buf[32];

	ret = qp_check(qp);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", qp);
	
	ret = set_property(cfg_qp_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int rcmode_check(int rcmode)
{
	if((rcmode == RC_MODE_NORMAL_MODE) || (rcmode == RC_MODE_FLUENCY_FIRST_MODE))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_rcmode_get(int *rcmode)
{
#ifdef LINUX_X86
	*rcmode = 0;
#else	
	int ret;
	char buf[32];
	*rcmode = 0;
	ret = get_property(cfg_rcmode_key, buf, 32);
	if(ret <= 0)
	{
		return -1;
	}
	if(strstr(buf, "0") == NULL && strstr(buf, "1") == NULL)
	{
		return -1;
	}
	ret = rcmode_check(atoi(buf));
	if(0 == ret)
	{
		return ret;
	}
	*rcmode = atoi(buf);
#endif
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_rcmode_set(int rcmode)
{
	int ret;
	char buf[32];
	ret = rcmode_check(rcmode);
	if(0 == ret)
	{
		return ret;
	}
	sprintf(buf, "%d", rcmode);

	ret = set_property(cfg_rcmode_key, buf);
	return ret;
}
////////////////////////////////////////////////////////////////
int cfg_codec_set(int codec)
{
	int ret;
	char buf[32];
	
	ret = codec_check(codec);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", codec);
	
	ret = set_property(cfg_codec_key, buf);
		
	return ret;
}
////////////////////////////////////////////////////////////////
int gop_check(int gop)
{
	if((gop >= 1) &&  (gop <= 1000))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_gop_get(int *gop)
{
#ifdef LINUX_X86

	*gop = 60;
	return 1;

#else	

	int ret;
	char buf[32];
	
	*gop = 60;
	ret = get_property(cfg_gop_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}

	ret = gop_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*gop = atoi(buf);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_gop_set(int gop)
{
	int ret;
	char buf[32];

	ret = gop_check(gop);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", gop);
	
	ret = set_property(cfg_gop_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int fps_check(int fps)
{
	if((fps >= 1) &&  (fps <= 30))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_fps_get(int *fps)
{
#ifdef LINUX_X86
	*fps = 30; 	
	return 1;
#else	
	int ret;
	char buf[32];

	*fps = 30; 	
	ret = get_property(cfg_fps_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}

	ret = fps_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*fps = atoi(buf);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_fps_set(int fps)
{
	int ret;
	char buf[32];

	ret = fps_check(fps);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", fps);
	
	ret = set_property(cfg_fps_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int bps_check(int bps)
{
	// 100kbps <=> 20Mbps
	if((bps >= 100 * 1000) && (bps <= 20 * 1000 * 1000))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_bps_get(int *bps)
{
#ifdef LINUX_X86
	*bps = 2000 * 1000; // 2Mbps 	
	return 1;
#else	
	int ret;
	char buf[32];
	
	*bps = 2000 * 1000; // 2Mbps 	
	ret = get_property(cfg_bps_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	
	ret = bps_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	
	*bps = atoi(buf);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_bps_set(int bps)
{
	int ret;
	char buf[32];
	
	ret = bps_check(bps);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", bps);
	
	ret = set_property(cfg_bps_key, buf);
	
	return ret;
}

////////////////////////////////////////////////////////////////
int cfg_dev_sn_get(char *dev_sn, int max_size)
{

#ifdef LINUX_X86
	strcpy(dev_sn, "111222"); // 11 digit bit
	return 1;
#else	
	int ret;	
	ret = get_property(cfg_dev_sn_key, dev_sn, max_size);
	if(ret <= 0)
	{
		return ret;
	}
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_video_id_get(char *video_id, int max_size)
{

#ifdef LINUX_X86
	strcpy(video_id, "888888321"); 
	return 1;
#else	
	int ret;	
	ret = get_property(cfg_video_id_key, video_id, max_size);
	if(ret <= 0)
	{
		return ret;
	}
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_dev_type_get(int *dev_type)
{

#ifdef LINUX_X86
	*dev_type = MESH_DEV_T_Z800AG;
	return 1;
#else	
	int ret;	
	char buf[16];
	ret = get_property(cfg_dev_d10x_key, buf, 16);
	if(ret <= 0)
	{
		// If getting dev_type failed, we assume it is a 9300M device
		*dev_type = MESH_DEV_T_9300M;
		return 1;
	}
	ret       = atoi(buf);
	*dev_type = mesh_dev_type_get(ret);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_dev_model_id_get(int *dev_type)
{
#ifdef LINUX_X86
	*dev_type = 16007026;
	return 1;
#else	
	int ret;	
	char buf[16];
	ret = get_property(cfg_dev_type_key, buf, 16);
	if(ret <= 0)
	{
		// If getting dev_type failed, we assume it is a 9300M device
		*dev_type = 16007026;
		return 1;
	}
	
	*dev_type       = atoi(buf);
#endif	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_video_enable_get(int *video_enable)
{

#ifdef LINUX_X86
	*video_enable = 1;
	return 1;
#else	
	int ret;	
	char buf[16];
	*video_enable = 0;
	ret = get_property(cfg_video_enable_key, buf, 16);
	if(ret <= 0)
	{
		return 0;
	}
	
	*video_enable = atoi(buf);
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_nickname_get(char *name, int max_size)
{

#ifdef LINUX_X86
	strcpy(name, "MT_test_nickname");
	return 1;
#else	
	int ret;	
	ret = get_property(cfg_dev_nickname_key, name, max_size);
	if(ret <= 0)
	{
		return ret;
	}
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_name_get(char *name, int max_size)
{

#ifdef LINUX_X86
	strcpy(name, "MT_test907;");
	return 1;
#else	
	int ret;	
	ret = get_property(cfg_dev_name_key, name, max_size);
	if(ret <= 0)
	{
		return ret;
	}
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int resolution_check(int w, int h)
{
	// 100kbps <=> 20Mbps
	if((w == 3840) && (h == 2160))  //4K
	{
		return 1;
	}
	else if((w == 2560) && (h == 1440)) //2K
	{
		return 1;
	}
	else if((w == 1920) && (h == 1080)) //1080
	{
		return 1;
	}
	else if((w == 1280) && (h == 720)) //720
	{
		return 1;
	}
	else if((w == 704) && (h == 576)) //D1
	{
		return 1;
	}	
	else if((w == 720) && (h == 576)) //D1
	{
		return 1;
	}
	else if((w == 720) && (h == 480)) //NTSC
	{
		return 1;
	}	
	else if((w == 640) && (h == 480)) 
	{
		return 1;
	}
	else if((w == 320) && (h == 240)) 
	{
		return 1;
	}
	
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_resolution_get(int *w, int *h)
{
#ifdef LINUX_X86
/*
	*w = 1920; 
	*h = 1080;	
*/	
	*w = 1280; 
	*h = 720;	
	return 1;
#else	
	int ret, x, y;
	char buf[32];

	*w = 1920; 
	*h = 1080;	
	ret = get_property(cfg_res_w_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	x = atoi(buf);
	
	ret = get_property(cfg_res_h_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}	
	y = atoi(buf);
	
	ret = resolution_check(x, y);
	if(0 == ret)
	{
		return 1;
	}
	
	*w = x;
	*h = y;
	
#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_resolution_set(int w, int h)
{
	int ret;
	char buf[32];

	ret = resolution_check(w, h);
	if(0 == ret)
	{
		return 0;
	}
	
	sprintf(buf, "%d", w);
	ret = set_property(cfg_res_w_key, buf);
	
	sprintf(buf, "%d", h);
	ret = set_property(cfg_res_h_key, buf);
	
	return ret;
}
////////////////////////////////////////////////////////////////
int text_size_check(int size)
{
	if((size != 16) && (size != 32) && (size != 48) && \
		(size != 64)  && (size != 96) && (size != 128)) 
	{
		return 0;
	}			
	return 1;
}

int text_size_check_for_bat(int size)
{
	if((size != 64) && (size != 96)) 
	{
		return 0;
	}			
	return 1;
}
////////////////////////////////////////////////////////////////
int position_check(int pos)
{
	if((pos >= 1) && (pos <= 4))
	{
		return 1;
	}
	return 0;
}
////////////////////////////////////////////////////////////////
int cfg_osd_get(int *timeStampShow, char *timeStampFormat, int *timeStampPosition, int *timeStampTextSize, int *timeStampMarginX, int *timeStampMarginY, 
				int *titleShow, int *titlePosition, int *titleTextSize, int *titleMarginX, int *titleMarginY, char *titleText, int *textLen,
				int *gnssStampShow, int *gnssStampPosition, int *gnssStampTextSize, int *gnssStampMarginX, int *gnssStampMarginY,
				int *batSigShow, int *batSigPosition, int *batSigTextSize, int *batSigMarginX, int *batSigMarginY)
{
#ifndef LINUX_X86	
	int ret;
	char buf[32];
#endif

	*timeStampShow = 1;
	strcpy(timeStampFormat, "yyyy-MM-dd HH:mm:ss"); 
	*timeStampPosition = 2;
	*timeStampTextSize = 64;
	*timeStampMarginX  = 50; 
	*timeStampMarginY  = 30; 
	
	*titleShow     = 1; 
	*titlePosition = 4;
	*titleTextSize = 48;
	*titleMarginX   = 50;
	*titleMarginY   = 30;
	char *test_str = "邻居信息闪现一次";
	strcpy(titleText, test_str);
	*textLen       = strlen(test_str);
	
#ifdef LINUX_X86
	return 1;
#else	
	// time osd params
	ret = get_property(cfg_time_show_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	*timeStampShow = atoi(buf);
	
	ret = get_property(cfg_time_format_key, timeStampFormat, 32); // TODO. len  check?
	if(ret <= 0)
	{
		return 1;
	}	

	ret = get_property(cfg_time_position_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	ret = position_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}			
	*timeStampPosition = atoi(buf);

	ret = get_property(cfg_time_text_size_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	ret = text_size_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	*timeStampTextSize = atoi(buf);
	
	ret = get_property(cfg_time_marginX_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*timeStampMarginX = atoi(buf);

	ret = get_property(cfg_time_marginY_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*timeStampMarginY = atoi(buf);
	
	// title osd params
	ret = get_property(cfg_title_show_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*titleShow = atoi(buf);	

	ret = get_property(cfg_title_position_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	ret = position_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}		
	*titlePosition = atoi(buf);	

	ret = get_property(cfg_title_text_size_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	ret = text_size_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}	
	*titleTextSize = atoi(buf);	

	ret = get_property(cfg_title_marginX_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*titleMarginX = atoi(buf);	
	
	ret = get_property(cfg_title_marginY_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*titleMarginY = atoi(buf);	
	
	*textLen = 0;
	ret = get_property(cfg_title_text_key, titleText, 256);
	if(ret <= 0)
	{
		return ret;
	}
	*textLen = strlen(titleText);
	
	// gnss osd params
	ret = get_property(cfg_gnss_show_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	*gnssStampShow = atoi(buf);
	
	ret = get_property(cfg_gnss_position_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	ret = position_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}			
	*gnssStampPosition = atoi(buf);

	ret = get_property(cfg_gnss_text_size_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	ret = text_size_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	*gnssStampTextSize = atoi(buf);
	
	ret = get_property(cfg_gnss_marginX_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*gnssStampMarginX = atoi(buf);	
	
	ret = get_property(cfg_gnss_marginY_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*gnssStampMarginY = atoi(buf);
	
	// bat sig osd params
	ret = get_property(cfg_bat_sig_show_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*batSigShow = atoi(buf);

	ret = get_property(cfg_bat_sig_position_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	ret = position_check(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	*batSigPosition = atoi(buf);

	ret = get_property(cfg_bat_sig_text_size_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	ret = text_size_check_for_bat(atoi(buf));
	if(0 == ret)
	{
		return 1;
	}
	*batSigTextSize = atoi(buf);

	ret = get_property(cfg_bat_sig_marginX_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*batSigMarginX = atoi(buf);

	ret = get_property(cfg_bat_sig_marginY_key, buf, 32);
	if(ret <= 0)
	{
		return ret;
	}
	*batSigMarginY = atoi(buf);

#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_osd_set(int timeStampShow, const char *timeStampFormat, int timeStampPosition, int timeStampTextSize, int timeStampMarginX, int timeStampMarginY, 
				int titleShow, int titlePosition, int titleTextSize, int titleMarginX, int titleMarginY, const char *titleText, int textLen,
				int gnssStampShow, int gnssStampPosition, int gnssStampTextSize, int gnssStampMarginX, int gnssStampMarginY,
				int batSigShow, int batSigPosition, int batSigTextSize, int batSigMarginX, int batSigMarginY)
{
	int ret;
	char buf[32];

	ret = text_size_check(timeStampTextSize);
	if(0 == ret)
	{
		return 0;
	}

	ret = text_size_check(titleTextSize);
	if(0 == ret)
	{
		return 0;
	}
	
	ret = position_check(titlePosition);
	if(0 == ret)
	{
		return 0;
	}		
	ret = position_check(timeStampPosition);
	if(0 == ret)
	{
		return 0;
	}		
	
	//time
	sprintf(buf, "%d", timeStampShow);
	ret = set_property(cfg_time_show_key, buf);
	
	ret = set_property(cfg_time_format_key, timeStampFormat);
	
	sprintf(buf, "%d", timeStampPosition);
	ret = set_property(cfg_time_position_key, buf);
	
	sprintf(buf, "%d", timeStampTextSize);
	ret = set_property(cfg_time_text_size_key, buf);
	
	sprintf(buf, "%d", timeStampMarginX);
	ret = set_property(cfg_time_marginX_key, buf);

	sprintf(buf, "%d", timeStampMarginY);
	ret = set_property(cfg_time_marginY_key, buf);
	
	//title
	sprintf(buf, "%d", titleShow);
	ret = set_property(cfg_title_show_key, buf);
	
	sprintf(buf, "%d", titlePosition);
	ret = set_property(cfg_title_position_key, buf);
	
	sprintf(buf, "%d", titleTextSize);
	ret = set_property(cfg_title_text_size_key, buf);

	sprintf(buf, "%d", titleMarginX);
	ret = set_property(cfg_title_marginX_key, buf);

	sprintf(buf, "%d", titleMarginY);
	ret = set_property(cfg_title_marginY_key, buf);
	
	//chinese text utf-8 encode
	ret = set_property(cfg_title_text_key, titleText);
	
	// gnss osd
	sprintf(buf, "%d", gnssStampShow);
	ret = set_property(cfg_gnss_show_key, buf);
		
	sprintf(buf, "%d", gnssStampPosition);
	ret = set_property(cfg_gnss_position_key, buf);
	
	sprintf(buf, "%d", gnssStampTextSize);
	ret = set_property(cfg_gnss_text_size_key, buf);
	
	sprintf(buf, "%d", gnssStampMarginX);
	ret = set_property(cfg_gnss_marginX_key, buf);	
	
	sprintf(buf, "%d", gnssStampMarginY);
	ret = set_property(cfg_gnss_marginY_key, buf);

	// bat sig osd
	sprintf(buf, "%d", batSigShow);
	ret = set_property(cfg_bat_sig_show_key, buf);

	sprintf(buf, "%d", batSigPosition);
	ret = set_property(cfg_bat_sig_position_key, buf);

	sprintf(buf, "%d", batSigTextSize);
	ret = set_property(cfg_bat_sig_text_size_key, buf);

	sprintf(buf, "%d", batSigMarginX);
	ret = set_property(cfg_bat_sig_marginX_key, buf);

	sprintf(buf, "%d", batSigMarginY);
	ret = set_property(cfg_bat_sig_marginY_key, buf);


	
	return ret;
}
////////////////////////////////////////////////////////////////
// model_id definition ref http://trac.gbcom.com.cn/SpiderNet
u_int8_t mesh_dev_type_get(int model_id) 
{
    int model_type = 0;
	int ret = 0;
	if(1 == model_id)
	{
		return MESH_DEV_T_9300M;
	}
	else
	{
		ret = cfg_dev_model_id_get(&model_type);
		if(16004011 == model_type)
		{
			return MESH_DEV_T_Z800AH;
		}
		else if(model_type == 16004013)
		{
			return MESH_DEV_T_Z800AG;
		}		
		else if(16004012 == model_type)
		{
			//wangyun 20260106 Z900跟Z800BG逻辑一致，不新增设备类型了
			return MESH_DEV_T_Z800BG;
		}
		else if(16004021 == model_type)
		{
			return MESH_DEV_T_Z900;
		}
		else if(model_type = 16007041)
		{
			return MESH_DEV_T_XR9700;
		}
		else if(model_type == 16004014)
		{
			return MESH_DEV_T_Z800BH;
		}
		else
		{
			//return MESH_DEV_T_Z800BG;
			return MESH_DEV_T_XM2300; //若非Z800单兵系列，则认为是模块系列
		}
	} 
	
	return 	MESH_DEV_BASE_STATION;
}
////////////////////////////////////////////////////////////////
int audio_samples_set(int samples)
{
	char cmd[256];
	
	memset(cmd, 0, 256);
	sprintf(cmd, "echo %d > %s", samples, AV_AUDIO_SAMPLES_FILE);

#ifdef LINUX_X86
	system(cmd);
#else		
	android_system(cmd);	
#endif	

	return 1;
}
////////////////////////////////////////////////////////////////
int audio_samples_get(int *samples)
{
	FILE *fp;
	char buf[64];
	int len, ret;
	
	fp = fopen(AV_AUDIO_SAMPLES_FILE, "r");
	if(!fp)
	{
		return 0;
	}
	
	len = fread(buf, 1, 64, fp);
	fclose(fp);
	if(len <= 0 )
	{
		return 0;
	}
	
	*samples = atoi(buf);
	ret = samples_check(*samples);
	if(!ret)
	{
		return 0;
	}

	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_ptt_multicast_port_set(int port)
{
	int ret;
	char buf[32];

	if((port <= 0) || (port > 65535))
	{
		return 0;
	}
	
	sprintf(buf, "%d", port);
	
	ret = set_property(cfg_ptt_multicast_port_key, buf);
		
	return ret;
}
////////////////////////////////////////////////////////////////
int cfg_ptt_multicast_port_get(int *port)
{
#ifdef LINUX_X86
	
	*port = DEFAULT_RTP_MULTICAST_PORT;
	return 1;
	
#else	
	
	int ret;
	char buf[32];

	*port = DEFAULT_RTP_MULTICAST_PORT;

	ret = get_property(cfg_ptt_multicast_port_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	
	ret = atoi(buf);
	if((ret > 0) && (ret < 65535))
	{
		*port = ret;
	}

#endif
	
	return 1;
}

////////////////////////////////////////////////////////////////
int cfg_ptt_unicast_port_set(int port)
{
	int ret;
	char buf[32];

	if((port <= 0) || (port > 65535))
	{
		return 0;
	}
	
	sprintf(buf, "%d", port);
	
	ret = set_property(cfg_ptt_unicast_port_key, buf);
		
	return ret;
}
////////////////////////////////////////////////////////////////
int cfg_ptt_unicast_port_get(int *port)
{
#ifdef LINUX_X86
	
	*port = DEFAULT_RTP_UNICAST_PORT;
	return 1;
	
#else	
	
	int ret;
	char buf[32];

	*port = DEFAULT_RTP_UNICAST_PORT;

	ret = get_property(cfg_ptt_unicast_port_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	
	ret = atoi(buf);
	if((ret > 0) && (ret < 65535))
	{
		*port = ret;
	}

#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
int cfg_ptt_priority_set(int priority)
{
	int ret;
	char buf[32];
	
	sprintf(buf, "%d", priority);
	
	ret = set_property(cfg_ptt_priority_key, buf);
		
	return ret;
}
////////////////////////////////////////////////////////////////
int cfg_ptt_priority_get(int *priority)
{
#ifdef LINUX_X86
	
	*priority = 0;
	return 1;
	
#else	
	
	int ret;
	char buf[32];

	*priority = 0;

	ret = get_property(cfg_ptt_priority_key, buf, 32);
	if(ret <= 0)
	{
		return 1;
	}
	
	*priority = atoi(buf);

#endif
	
	return 1;
}
////////////////////////////////////////////////////////////////
////////////////////////码率控制规则配置/////////////////////////
int outline_qp_calculate(int w,int h,int fps,int code_rate)
{
    double qp_calcu = 0.0;
    int qp_set = 0;
    
    double pixel_total = (double)w * (double)h * (double)fps;

    if (pixel_total < 1.0) return 26; // 默认值

    qp_calcu = -8.658 * log(((double)code_rate) / pixel_total);


    return (int)qp_calcu;
}

////////////////////////////////////////////////////////////////
int qp_calculate(av_video_cfg_t *av_cfg)
{
    double qp_calcu = 0.0;
    int qp_set = 0;
    
    // 保护除零
    double pixel_total = (double)av_cfg->w * (double)av_cfg->h * (double)av_cfg->fps;
    if (pixel_total < 1.0) return 26; // 默认值

    qp_calcu = -8.658 * log(((double)av_cfg->code_rate) / pixel_total);
    
    // 建议修改：为了提升静止画质，减少 offset，例如改为 +2 或 +0
    // 原公式：qp_set = floor(qp_calcu) + 5; 
    qp_set = (int)qp_calcu; // 稍微降低一点下限，给画质留空间

    // 安全钳位（H.264 QP 范围通常是 0-51，但作为 MinQP 建议限制在合理范围）
    if (qp_set < 15) qp_set = 15; // 别太低，否则码率很难压住
    if (qp_set > MAX_QP_MIN_VALUE) qp_set = MAX_QP_MIN_VALUE; // 别太高，否则画面烂得没法看

    return qp_set;
}
////////////////////////////////////////////////////////////////
/*
property_set/property_get位于libcutils.so库。任何进程若要调用这两个函数，需要链接libcutils.so。
libcutils.so调用到了__system_property_set和__system_property_get函数，这两个函数位于bionic库中，源代码文件为bionic/libc/bionic/system_properties.c，生成的库文件为libc_common.so。
对于set和get，分别会创建socket，发送消息到服务端。服务端位于init进程中，有一个property_service进程专门负责这个。
该service读取socket消息，进行set和get的处理。property_service代码位于system/core/init/property_service.c
在system_properties.c和property_service.c中都用到了共享内存(用mmap实现）

启动属性服务时，将从以下文件中加载默认属性：
/default.prop
/system/build.prop 
/system/default.prop
/data/local.prop

shell中可以使用 getprop / setprop 命令来操作.

******* 为了避免库连接问题，属性采用 命令 的方式来操作。

persist.gb.net.dev_sn
ro.gb.product.Innermodel
ro.gb.product.sysDeviceType
ro.gb.product.CustomerSN
ro.gb.cap.videoModule
persist.gb.sys.device_name

*/

