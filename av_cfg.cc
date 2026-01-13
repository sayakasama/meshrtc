#include <string>

#include "mesh_rtc_inc.h"

#include "cjson.h"
#include "json_codec.h"

#include "cfg_28181.h"
#include "cfg.h"
#include "av_api.h"
#include "av_cfg.h"


#define	RESULOTION_4K_WIDTH  3840
#define	RESULOTION_4K_HEIGHT 2160
//////////////////////////////////////////////////////////////////////////////////
extern av_params_t av_params;
int av_wh_limited   = 0;

// screen w*h=640*480 and encoder require 32 byte align, so we use 608*352
int screen_width_max  = 608;
int screen_height_max = 352;

const char *rtc_server_addr_file = "/tmp/rtc_server_addr";
//////////////////////////////////////////////////////////////////////////////////
void av_wh_limit_reset(int is_limited)
{
	av_wh_limited = is_limited;
}
//////////////////////////////////////////////////////////////////////////////////
void av_wh_max_set(int w, int h)
{
	screen_width_max  = w;
	screen_height_max = h;	
}
//////////////////////////////////////////////////////////////////////////////////
void  av_wh_max_get(int *w, int *h)
{
	if(av_wh_limited)
	{
		*w = screen_width_max;
		*h = screen_height_max;		
	} else {
		*w = RESULOTION_4K_WIDTH;
		*h = RESULOTION_4K_HEIGHT;		
	}
	
	return;
}
//////////////////////////////////////////////////////////////////////////////////
int av_tmp_rtc_server_save(const char *rtc_server)
{
	//char cmd[128];
	FILE *fp;

	if(!rtc_server)
	{
		return 0;
	}
	
	fp = fopen(rtc_server_addr_file, "w+");
	if(!fp)
	{
		return -1;
	}
	
	fwrite(rtc_server, 1, strlen(rtc_server), fp);
	fclose(fp);
	
	if(strlen(rtc_server))
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " clean current rtc_server  ");		
	} else {
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " save current rtc_server %s ", rtc_server);
	}
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_tmp_rtc_server_restore(char *rtc_server, int max_size)
{
	//char cmd[128];
	FILE *fp;
	int ret;
	
	if(!rtc_server)
	{
		return -1;
	}
	
	rtc_server[0] = 0;
	fp = fopen(rtc_server_addr_file, "r");
	if(!fp)
	{
		return -1;
	}

	ret = fread(rtc_server, 1, max_size, fp);
	fclose(fp);		
	if(ret <= 0)
	{
		return -1;
	}

	if(strlen(rtc_server) <=  strlen("1.2.3.4")) // a simple check 
	{
		return -1;
	}
	
	MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " restore rtc_server %s ", rtc_server);						
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
void av_video_fps_get(int *fps)
{
	av_video_cfg_t *video_cfg = &av_params.video_cfg;

	*fps = video_cfg->fps; 	
}
//////////////////////////////////////////////////////////////////////////////////
void av_video_res_get(int *w, int *h)
{
	av_video_cfg_t *video_cfg = &av_params.video_cfg;

	*w = video_cfg->w; 
	*h = video_cfg->h;
	if(( *w == 0 ) || ( *h == 0 ))
	{
		*w = screen_width_max;
		*h = screen_height_max;			
	}	
}
//////////////////////////////////////////////////////////////////////////////////
int av_audio_cfg_hton(av_audio_cfg_t *cfg_in, av_audio_cfg_t *cfg_out)
{
	if((NULL == cfg_in) || (NULL == cfg_out))
	{
		return 0;
	}
	
#if 1
	memcpy(cfg_out, cfg_in, sizeof(av_audio_cfg_t));
#else
	cfg_out->enable  = htonl(cfg_in->enable);
	cfg_out->samples = htonl(cfg_in->samples);
	cfg_out->aec     = htonl(cfg_in->aec);
	cfg_out->volume  = htonl(cfg_in->volume);	
#endif
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_video_cfg_hton(av_params_t *cfg_in, av_params_t *cfg_out)
{
	av_video_cfg_t *video_cfg_in, *video_cfg_out;
	av_osd_cfg_t   *osd_cfg_in, *osd_cfg_out;
	
	if((NULL == cfg_in) || (NULL == cfg_out))
	{
		return 0;
	}
	
#if 1
	memcpy(cfg_out, cfg_in, sizeof(av_params_t));
#else	
	video_cfg_in  = &cfg_in->video_cfg;
	video_cfg_out = &cfg_out->video_cfg;
	osd_cfg_in    = &cfg_in->osd_cfg;
	osd_cfg_out   = &cfg_out->osd_cfg;
	
	//video convert
	video_cfg_out->qp    = htonl(video_cfg_in->qp);
	video_cfg_out->gop   = htonl(video_cfg_in->gop);
	video_cfg_out->fps   = htonl(video_cfg_in->fps);
	video_cfg_out->w     = htonl(video_cfg_in->w);
	video_cfg_out->h     = htonl(video_cfg_in->h);
	video_cfg_out->codec = htonl(video_cfg_in->codec);	
	video_cfg_out->code_rate  = htonl(video_cfg_in->code_rate);
	
	// osd convert
	// time
	osd_cfg_out->time_show = htonl(osd_cfg_in->time_show);
	osd_cfg_out->time_position = htonl(osd_cfg_in->time_position);
	osd_cfg_out->time_text_size = htonl(osd_cfg_in->time_text_size);
	osd_cfg_out->time_margin    = htonl(osd_cfg_in->time_margin);
	osd_cfg_out->time_offset_x  = htonl(osd_cfg_in->time_offset_x);
	osd_cfg_out->time_offset_y  = htonl(osd_cfg_in->time_offset_y);
	
	//title
	osd_cfg_out->title_show      = htonl(osd_cfg_in->title_show);
	osd_cfg_out->title_position  = htonl(osd_cfg_in->title_position);
	osd_cfg_out->title_text_size = htonl(osd_cfg_in->title_text_size);
	osd_cfg_out->title_margin    = htonl(osd_cfg_in->title_margin);
	osd_cfg_out->text_offset_x   = htonl(osd_cfg_in->text_offset_x);
	osd_cfg_out->text_offset_y   = htonl(osd_cfg_in->text_offset_y);
	// title text	
	osd_cfg_out->text_len        = htonl(osd_cfg_in->text_len);	
#endif	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_video_cfg_get(av_params_t *av_params)
{
	int ret;
	av_osd_cfg_t *osd         = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;
	
	memset(av_params, 0, sizeof(av_params_t));
	
	ret = cfg_codec_get(&video_cfg->codec);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_codec_get error");						
		return ret;
	}
	
	ret = cfg_gop_get(&video_cfg->gop);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_gop_get error");								
		return ret;
	}
	
	ret = cfg_qp_get(&video_cfg->qp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_qp_get error");								
		return ret;
	}
	
	ret = cfg_fps_get(&video_cfg->fps);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_fps_get error");								
		return ret;
	}
	
	ret = cfg_bps_get(&video_cfg->code_rate);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_bps_get error");								
		return ret;
	}
	
	ret = cfg_resolution_get(&video_cfg->w, &video_cfg->h);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_resolution_get error");								
		return ret;
	}
		
	ret = cfg_osd_get(&osd->time_show, osd->time_format, &osd->time_position, &osd->time_text_size, &osd->time_offset_x, &osd->time_offset_y, 
						&osd->title_show, &osd->title_position, &osd->title_text_size, &osd->text_offset_x, &osd->text_offset_y, osd->title_text, &osd->text_len,
						&osd->gnss_show, &osd->gnss_position, &osd->gnss_text_size, &osd->gnss_offset_x, &osd->gnss_offset_y,
						&osd->bat_sig_show, &osd->bat_sig_position, &osd->bat_sig_text_size, &osd->bat_sig_offset_x, &osd->bat_sig_offset_y);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_osd_get error");								
		return ret;
	}

	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_video_cfg_set(av_params_t *av_params)
{
	av_osd_cfg_t       *osd   = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;
	int ret;

	ret = cfg_codec_set(video_cfg->codec);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_codec_set error: invalid codec=%d", video_cfg->codec);
		return ret;
	}
	
	ret = cfg_qp_set(video_cfg->qp);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_qp_set error: invalid qp=%d", video_cfg->qp);		
		return ret;
	}
	
	ret = cfg_gop_set(video_cfg->gop);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_gop_set error: invalid gop=%d", video_cfg->gop);		
		return ret;
	}
	
	ret = cfg_fps_set(video_cfg->fps);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_fps_set error: invalid fps=%d", video_cfg->fps);
		return ret;
	}
	
	ret = cfg_bps_set(video_cfg->code_rate);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_bps_set error: invalid bps=%d", video_cfg->code_rate);	
		return ret;
	}
	
	ret = cfg_resolution_set(video_cfg->w, video_cfg->h);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_resolution_set error: w=%d h=%d ", video_cfg->w, video_cfg->h);
		return ret;
	}
		
	ret = cfg_osd_set(osd->time_show, osd->time_format, osd->time_position, osd->time_text_size, osd->time_offset_x, osd->time_offset_y,
						osd->title_show, osd->title_position, osd->title_text_size, osd->text_offset_x, osd->text_offset_y, osd->title_text, osd->text_len,
						osd->gnss_show, osd->gnss_position, osd->gnss_text_size, osd->gnss_offset_x, osd->gnss_offset_y,
						osd->bat_sig_show, osd->bat_sig_position, osd->bat_sig_text_size, osd->bat_sig_offset_x, osd->bat_sig_offset_y);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_osd_set error");		
		return ret;
	}
	
	return 1;
}

//////////////////////////////////////////////////////////////////////////////////
int av_audio_cfg_get(av_audio_cfg_t *audio_cfg)
{
	int ret;
	
	audio_cfg->enable = 1;
	ret = cfg_samples_get((int *)&audio_cfg->samples);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_samples_get error");		
		return ret;
	}
	
	ret = cfg_aec_get((int *)&audio_cfg->aec);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_aec_get error");		
		return ret;
	}
	
	ret = cfg_volume_get((int *)&audio_cfg->volume);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_volume_get error");		
		return ret;
	}
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_audio_cfg_set(av_audio_cfg_t *audio_cfg)
{
	int ret;

	audio_cfg->enable = 1;
	ret = cfg_samples_set(audio_cfg->samples);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_samples_set error");				
		return ret;
	}
	
	ret = cfg_aec_set(audio_cfg->aec);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_aec_set error");				
		return ret;
	}
	
	ret = cfg_volume_set(audio_cfg->volume);
	if(ret <= 0)
	{
		MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, " cfg_volume_set error");				
		return ret;
	}
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int av_osd_text_set(char *text, int len)
{
	const char *photo_file = "/tmp/osd.bmp";
	FILE *fp;
	char *p = (char *)malloc(1024*1024); // max 1M byte
	int ret;
	
	if(!p) 
	{
		return -1;
	}
	
	if(strlen(text) == 0)
	{
		// clean text ?
	}
	
	// convert text to photo file.  this function is not used in release version
	
	// read data from photo file
	fp = fopen(photo_file, "r+");
	if(!fp)
	{
		free(p);
		return 1;
	}
	
	ret = fread(p, 1, 1024*1024, fp);
	if(ret <= 0)
	{
		free(p);
		fclose(fp);
		return -1;
	}
		
	osd_text_set(p, ret);	
	
	free(p);
	fclose(fp);	
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
std::string  fluentfisrtEncode()
{
	int rcmode = 0;
	int ret = 0;
	std::string fluentfisrt_str;
/*
		“fluentFirstMode”: { //流畅优先模式
			“value”: “true” // enable
		}, 
*/
	ret = cfg_rcmode_get(&rcmode);
	if(rcmode == 1){
		fluentfisrt_str = "\"fluentFirstMode\": {  \"value\": \"true\"    },  ";
	}
	else{
		fluentfisrt_str = "\"fluentFirstMode\": {  \"value\": \"false\"    },  ";
	}

	return fluentfisrt_str;
}
//////////////////////////////////////////////////////////////////////////////////
std::string  bitRateEncode(int bitrate)
{
	std::string bps_str;
/*
		“bitRate”: { //码率
			“value”: “500” // 500 Kbps
		}, 
*/
	bps_str = "\"bitRate\": {  \"value\": \"" + std::to_string(bitrate) +"\"     },  "; // 500 Kbps
	
	return bps_str;
}
//////////////////////////////////////////////////////////////////////////////////
int  bitRateDecode(cJSON *info, int * bitrate)
{
	cJSON *item = cJSON_GetObjectItem(info, "bitRate");
	if(!item)
	{
		return 0;
	}
		
	cJSON *value = cJSON_GetObjectItem(item, "value");
	if(!value)
	{
		return 0;
	}
	
	*bitrate = atoi(value->valuestring);
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
std::string  resolutionEncode(int w, int h)
{
	std::string res_str;
/*
	“resolution”: { //分辨率
		“value”: “640*480” //宽*高
	},
*/
	res_str = "\"resolution\": {  \"value\": \"" + std::to_string(w) + "*" + std::to_string(h) + "\"     },  "; // 500 Kbps
	
	return res_str;	
}
//////////////////////////////////////////////////////////////////////////////////
int   resolutionDecode(cJSON *info, int *w, int *h)
{
	cJSON *item = cJSON_GetObjectItem(info, "resolution");
	if(!item)
	{
		return 0;
	}
		
	cJSON *value = cJSON_GetObjectItem(item, "value");
	if(!value)
	{
		return 0;
	}
	
	int n = sscanf(value->valuestring, "%d*%d", w, h);
	if(n == 2)
	{
		return 1;
	} else {
		return 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////
std::string  fpsEncode(int fps)
{
	std::string fps_str;
/*
	“fps”: { 
		“value”: “25”
	},
*/
	fps_str = "\"fps\": {  \"value\": \"" + std::to_string(fps) +"\"     },  "; // 

	return fps_str;	
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int  fpsDecode(cJSON *info, int * fps)
{
	cJSON *item = cJSON_GetObjectItem(info, "fps");
	if(!item)
	{
		return 0;
	}
		
	cJSON *value = cJSON_GetObjectItem(item, "value");
	if(!value)
	{
		return 0;
	}
	
	// server send two kind of msg: string & int. 
	if(value->valuestring)
	{
		*fps = atoi(value->valuestring);	
	} else {
		*fps = value->valueint;	
	}
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::string  osdEncode(int timeStampShow, const char *timeStampFormat, int timeStampPosition, int timeStampTextSize, int time_offset_x,  int time_offset_y, 
				int titleShow, int titlePosition, int titleTextSize, int text_offset_x, int text_offset_y, const char *titleText, int textLen,
				int gnssStampShow, int gnssStampPosition, int gnssStampTextSize, int gnssStampMarginX, int gnssStampMarginY,
				int batSigShow, int batSigPosition, int batSigTextSize, int batSigMarginX, int batSigMarginY)
{
	std::string  osd;
	std::string  timeStampShow_str;
	std::string  timeStampFormat_str;
	std::string  timeStampPosition_str; 
	std::string  timeStampTextSize_str;
	std::string  timeStampMarginX_str;
	std::string  timeStampMarginY_str;
	
	std::string  titleShow_str;
	std::string  titlePosition_str; 
	std::string  titleTextSize_str;
	std::string  titleText_str;
	std::string  titleMarginX_str;
	std::string  titleMarginY_str;

	std::string  gnssShow_str;
	std::string  gnssPosition_str;
	std::string  gnssTextSize_str;
	std::string  gnssMarginX_str;
	std::string  gnssMarginY_str;

	std::string  batSigShow_str;
	std::string  batSigPosition_str;
	std::string  batSigTextSize_str;
	std::string  batSigMarginX_str;
	std::string  batSigMarginY_str;


	
/*
		“osd”: { //osd设置
			“timeStampShow“: true, 						//是否显示时间戳
			“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
			“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
			“timeStampTextSize“: 25,						//时间戳字体大小
			“timeStampMargin“: 25,						//时间戳水印的边距
			“titleShow“: true, 						//是否显示标题文字
			“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
			“titleTextSize“: 25,						//标题字体大小
			“titleMargin“: 25,						//标题水印的边距
			“titleText”:”aa”,                       //标题文字
			\"timeStampMarginX\":\"1\",
			\"timeStampMarginY\":\"1\",
			\"titleMarginX\":\"1\",
			\"titleMarginY\":\"1\",			
		}
*/	
	if(timeStampShow) {
		timeStampShow_str = "\"timeStampShow\":" + std::string("true") + ",  ";
	} else {
		timeStampShow_str = "\"timeStampShow\":" + std::string("false") + ",  ";
	}
	
	timeStampFormat_str   = "\"timeStampFormat\": \"" + std::string(timeStampFormat) + "\",  ";
	
	timeStampPosition_str = "\"timeStampPosition\":"  + std::to_string(timeStampPosition) + ",  ";
	timeStampTextSize_str = "\"timeStampTextSize\":"  + std::to_string(timeStampTextSize) + ",  ";
	timeStampMarginX_str = "\"timeStampMarginX\":"    + std::to_string(time_offset_x) + ",  ";
	timeStampMarginY_str = "\"timeStampMarginY\":"    + std::to_string(time_offset_y) + ",  ";
	
	if(titleShow) {
		titleShow_str = "\"titleShow\":" + std::string("true") + ",  ";
	} else {
		titleShow_str = "\"titleShow\":" + std::string("false") + ",  ";
	}
	
	titlePosition_str = "\"titlePosition\":"  + std::to_string(titlePosition) + ",  ";
	titleTextSize_str = "\"titleTextSize\":"  + std::to_string(titleTextSize) + ",  ";
	titleMarginX_str  = "\"titleMarginX\":"   + std::to_string(text_offset_x) + ",  ";
	titleMarginY_str  = "\"titleMarginY\":"   + std::to_string(text_offset_y) + ",  ";

	titleText_str     = "\"titleText\": \"" + std::string(titleText) + "\"   "; // last json. chinese text utf-8 encode

	if(gnssStampShow) {
		gnssShow_str = "\"gnssShow\":" + std::string("true") + ",  ";
	} else {
		gnssShow_str = "\"gnssShow\":" + std::string("false") + ",  ";
	}

	gnssPosition_str = "\"gnssPosition\":"  + std::to_string(gnssStampPosition) + ",  ";
	gnssTextSize_str = "\"gnssTextSize\":"  + std::to_string(gnssStampTextSize) + ",  ";
	gnssMarginX_str  = "\"gnssMarginX\":"   + std::to_string(gnssStampMarginX) + ",  ";
	gnssMarginY_str  = "\"gnssMarginY\":"   + std::to_string(gnssStampMarginY) + ",  ";

	if(batSigShow) {
		batSigShow_str = "\"batSigShow\":" + std::string("true") + ",  ";
	} else {
		batSigShow_str = "\"batSigShow\":" + std::string("false") + ",  ";
	}
	batSigPosition_str = "\"batSigPosition\":"  + std::to_string(batSigPosition) + ",  ";
	batSigTextSize_str = "\"batSigTextSize\":"  + std::to_string(batSigTextSize) + ",  ";
	batSigMarginX_str  = "\"batSigMarginX\":"   + std::to_string(batSigMarginX) + ",  ";
	batSigMarginY_str  = "\"batSigMarginY\":"   + std::to_string(batSigMarginY) + ",  ";


	
	osd = "\"osd\": {  " + timeStampShow_str + timeStampFormat_str + timeStampPosition_str + timeStampTextSize_str + timeStampMarginX_str + timeStampMarginY_str + \
					titleShow_str + titlePosition_str + titleTextSize_str + titleMarginX_str + titleMarginY_str +  \
					gnssShow_str + gnssPosition_str + gnssTextSize_str + gnssMarginX_str + gnssMarginY_str + \
					batSigShow_str + batSigPosition_str + batSigTextSize_str + batSigMarginX_str + batSigMarginY_str + titleText_str + " }";
	
	return osd;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int  osdDecode(cJSON *info, av_osd_cfg_t *osd)
{
	cJSON *item;
	cJSON *item_osd = cJSON_GetObjectItem(info, "osd");

	if(!item_osd)
	{
		return 0;
	}

	// time osd params decode
	item = cJSON_GetObjectItem(item_osd, "timeStampShow");
	if(item)
	{
		osd->time_show = item->valueint;
	}
	
	item = cJSON_GetObjectItem(item_osd, "timeStampFormat");
	if(item)
	{
		if(item->valuestring)
			strcpy(osd->time_format, item->valuestring);
	}
	
	item = cJSON_GetObjectItem(item_osd, "timeStampPosition");
	if(item)
	{
		if(item->valuestring)
		{
			osd->time_position = atoi(item->valuestring);
		}
	}
	
	item = cJSON_GetObjectItem(item_osd, "timeStampTextSize");
	if(item)
	{
		if(item->valuestring)
		{
			osd->time_text_size = atoi(item->valuestring);
		}
	}	

	item = cJSON_GetObjectItem(item_osd, "timeStampMarginX");
	if(item)
	{
		if(item->valuestring)
			osd->time_offset_x = atoi(item->valuestring);
	}
	
	item = cJSON_GetObjectItem(item_osd, "timeStampMarginY");
	if(item)
	{
		if(item->valuestring)
			osd->time_offset_y = atoi(item->valuestring);
	}			

	// title osd params decode
	item = cJSON_GetObjectItem(item_osd, "titleShow");
	if(item)
	{
		osd->title_show = item->valueint;
	}

	item = cJSON_GetObjectItem(item_osd, "titlePosition");
	if(item)
	{
		if(item->valuestring)
		{
			osd->title_position = atoi(item->valuestring);
		}		
	}

	item = cJSON_GetObjectItem(item_osd, "titleTextSize");
	if(item)
	{
		if(item->valuestring)
		{
			osd->title_text_size = atoi(item->valuestring);
		}	
	}		
	
	item = cJSON_GetObjectItem(item_osd, "titleMarginX");
	if(item)
	{
		if(item->valuestring)
			osd->text_offset_x = atoi(item->valuestring);
	}
	
	item = cJSON_GetObjectItem(item_osd, "titleMarginY");
	if(item)
	{
		if(item->valuestring)
			osd->text_offset_y = atoi(item->valuestring);
	}

	item = cJSON_GetObjectItem(item_osd, "titleText");
	if(item)
	{
		if(item->valuestring)
		{
			strcpy(osd->title_text, item->valuestring); // chinese text utf-8 encode .
			osd->text_len = strlen(osd->title_text);
		} else {
			osd->text_len = 0;
		}
	}
	
	// gnss osd params decode
	item = cJSON_GetObjectItem(item_osd, "gnssShow");
	if(item)
	{
		osd->gnss_show = item->valueint;
	}

	item = cJSON_GetObjectItem(item_osd, "gnssMarginX");
	if(item)
	{
		if(item->valuestring)
			osd->gnss_offset_x = atoi(item->valuestring);
	}
	
	item = cJSON_GetObjectItem(item_osd, "gnssMarginY");
	if(item)
	{
		if(item->valuestring)
			osd->gnss_offset_y = atoi(item->valuestring);
	}	

	item = cJSON_GetObjectItem(item_osd, "gnssPosition");
	if(item)
	{
		if(item->valuestring)
		{
			osd->gnss_position = atoi(item->valuestring);
		}
	}
	
	item = cJSON_GetObjectItem(item_osd, "gnssTextSize");
	if(item)
	{
		if(item->valuestring)
		{
			osd->gnss_text_size = atoi(item->valuestring);
		}
	}

	// bat sig osd params decode
	item = cJSON_GetObjectItem(item_osd, "batSigShow");
	if(item)
	{
		osd->bat_sig_show = item->valueint;
	}

	item = cJSON_GetObjectItem(item_osd, "batSigMarginX");
	if(item)
	{
		if(item->valuestring)
			osd->bat_sig_offset_x = atoi(item->valuestring);
	}

	item = cJSON_GetObjectItem(item_osd, "batSigMarginY");
	if(item)
	{
		if(item->valuestring)
			osd->bat_sig_offset_y = atoi(item->valuestring);
	}

	item = cJSON_GetObjectItem(item_osd, "batSigPosition");
	if(item)
	{
		if(item->valuestring)
		{
			osd->bat_sig_position = atoi(item->valuestring);
		}
	}

	item = cJSON_GetObjectItem(item_osd, "batSigTextSize");
	if(item)
	{
		if(item->valuestring)
		{
			osd->bat_sig_text_size = atoi(item->valuestring);
		}
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
请求
{
    “action”: “deviceConfigQueryReplyMsg”,
	“messageId”: “这个信息的随机id”,
	“token”: “sdfasdfsd”,
	“param”: {
		“eventId”:” deviceConfigQueryMsg命令发来的标志”
		“bitRate”: { //码率
			“value”: “500” // 500 Kbps
		}, 
		“resolution”: { //分辨率
			“value”: “640*480” //宽*高
		},
		“fps”: { //帧率
			“value”: “25”
		},
		“osd”: { //osd设置
			“timeStampShow“: true, 						//是否显示时间戳
			“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
			“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
			“timeStampTextSize“: 25,						//时间戳字体大小
			“timeStampMargin“: 25,						//时间戳水印的边距
			“titleShow“: true, 						//是否显示标题文字
			“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
			“titleTextSize“: 25,						//标题字体大小
			“titleMargin“: 25,						//标题水印的边距
		}
	}
}
*/
std::string CfgQueryRspEncode(std::string msg_id, int fps_flag, int bps_flag, int res_flag, int osd_flag, av_params_t *av_params) 
{
	std::string query_rsp, head_str, params_str;
	std::string fps_str, bps_str, res_str, osd_str;
	av_osd_cfg_t       *osd   = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;
	
	if(fps_flag)
	{
		fps_str = fpsEncode(video_cfg->fps);
	} else {
		fps_str = "";
	}
	
	if(bps_flag)
	{
		bps_str = bitRateEncode(video_cfg->code_rate);
	} else {
		bps_str = "";
	}
	
	if(res_flag)
	{
		res_str = resolutionEncode(video_cfg->w, video_cfg->h);
	} else {
		res_str = "";
	}
	
	if(osd_flag)
	{
		osd_str = osdEncode(osd->time_show, osd->time_format, osd->time_position, osd->time_text_size, osd->time_offset_x,  osd->time_offset_y,
								osd->title_show, osd->title_position, osd->title_text_size, osd->text_offset_x,  osd->text_offset_y, osd->title_text, osd->text_len,
								osd->gnss_show, osd->gnss_position, osd->gnss_text_size, osd->gnss_offset_x, osd->gnss_offset_y,
								osd->bat_sig_show, osd->bat_sig_position, osd->bat_sig_text_size, osd->bat_sig_offset_x, osd->bat_sig_offset_y);
	} else {
		osd_str = "";
	}
	
	head_str   = "\"action\": \"deviceConfigQueryReplyMsg\",  \"messageId\": \"" + msg_id + "\",  \"token\": \"" + std::to_string(1234) + "\", ";
	params_str = "\"param\": {  " + fps_str + bps_str + res_str + osd_str + "  }";
	query_rsp  = "{ " + head_str + params_str + "  }";
	
	MESH_LOG_PRINT(PC_CLIENT_MODULE_NAME, LOG_INFO, "query_rsp:\n%s", query_rsp.c_str());
	
	return query_rsp;		
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
{
    “action”: “deviceConfigMsg”,
	“messageId”: “这个信息的随机id”,
	“info”: {
		“eventId”: “标志记录此次事件”;
		“bitRate”: { //码率
			“value”: “500” // 500 Kbps
		}, 
		“resolution”: { //分辨率
			“value”: “640*480” //宽*高
		},
		“fps”: { //帧率
			“value”: “25”
		},
		“osd”: { //osd设置
			“timeStampShow“: true, 						//是否显示时间戳
			“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
			“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
			“timeStampTextSize“: 25,						//时间戳字体大小
			“timeStampMargin“: 25,						//时间戳水印的边距
			“titleShow“: true, 						//是否显示标题文字
			“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
			“titleTextSize“: 25,						//标题字体大小
			“titleMargin“: 25,						//标题水印的边距
			“titleText”:”aa”                        //标题文字
		}
	}
}
*/
int CfgSetDecode(cJSON *info, av_params_t *av_params, char *event_id) 
{
	cJSON  *item_event;
	int ret;
	av_osd_cfg_t       *osd   = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;	
	int code_rate = 0;
	int w   = 640;
	int h   = 480;
	int fps = 25;

	item_event  = cJSON_GetObjectItem(info,  "eventId");
	if(!item_event)
	{
		return -1;
	}
	strcpy(event_id, item_event->valuestring);
		
	ret = bitRateDecode(info, &code_rate);
	if(ret == 1)
	{
		video_cfg->code_rate = code_rate;
	}
	
	ret = resolutionDecode(info, &w, &h);
	if(ret == 1)
	{
		video_cfg->w    = w;
		video_cfg->h    = h;
	}
	
	ret = fpsDecode(info, &fps);
	if(ret == 1)
	{
		video_cfg->fps  = fps;
	}
	
	ret = osdDecode(info, osd);

	return 1;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
{
    “action”: “gb28181ConfigMsg”,
	“messageId”: “这个信息的随机id”,
	“info”: {
		“eventId”: “标志记录此次事件”;
		“gb28181_params”: {
			“enable“: 1, 								//是否启用
			“istcp“: 1, 	                        	//信令传输协议 1：tcp 0：udp
			“server_type“: "wvp", 				    	//服务器类型 wvp/keda
			“stream_type“: 0,			            	//音视频流类型： 0 音频视频双流 1 仅视频流 2 音视频复合流（单流）
			“local_realm“: “3401000000”,		    	//设备SIP域
			“local_sip_id“:"34010000001180000009",  	//设备SIP ID
			“local_port“: 5060, 						//设备sip信令收发端口
			“local_av_ch“: "34010000001310000009",		//设备媒体通道ID（抽象出的媒体发送设备的编号）
			“peer_ip“: “10.30.2.8”,						//国标服务器IP地址
			“peer_port”:5060,	                    	//国标服务器sip信令收发端口
			“peer_realm”：“4401020049”,					//国标服务器SIP域
			“peer_sip_id”：“44010200492000000001”,		//国标服务器SIP ID
			“user”:"z800-28181-test",					//国标服务器登录用户名
			“passwd”:"admin123",						//国标服务器登录密码
			“hb_period”：20,							//心跳周期，单位秒
			"max_hb_ot":3,								//最大心跳超时次数
			“reg_period”：60，							//注册周期，单位秒
			"expire_time": 3600,						//注册过期时间，单位秒
			"adjust_time": 0							//视频码率自动调整周期，单位秒（默认0为不自动调整）
		}
	}
}
*/
int GB28181_CfgSetDecode(cJSON *info, gb_28181_cfg_t *gb28181_cfg, char *event_id) 
{
	cJSON  *item_event;
	cJSON  *item;
	int ret;

	item_event  = cJSON_GetObjectItem(info,  "eventId");
	if(!item_event)
	{
		return -1;
	}
	strcpy(event_id, item_event->valuestring);

	cJSON *item_gb28181_params = cJSON_GetObjectItem(info, "gb28181_params");
	if(!item_gb28181_params)
	{
		return -1;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "enable");
	if(item)
	{
		gb28181_cfg->gb28181_enable = item->valueint;
	}
	
	item = cJSON_GetObjectItem(item_gb28181_params, "istcp");
	if(item)
	{
		gb28181_cfg->is_tcp = item->valueint;
	}	

	item = cJSON_GetObjectItem(item_gb28181_params, "server_type");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->server_type, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "stream_type");
	if(item)
	{
		gb28181_cfg->video_stream_type = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "local_realm");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->local_realm, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "local_sip_id");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->local_sip_id, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "local_port");
	if(item)
	{
		gb28181_cfg->local_port = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "local_av_ch");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->local_av_channel, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "peer_ip");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->peer_ip, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "peer_port");
	if(item)
	{
		gb28181_cfg->peer_port = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "peer_realm");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->peer_realm, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "peer_sip_id");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->peer_sip_id, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "user");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->user_name, item->valuestring);
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "passwd");
	if(item)
	{
		if(item->valuestring)
			strcpy((char*)gb28181_cfg->passwd, item->valuestring);
	}	

	item = cJSON_GetObjectItem(item_gb28181_params, "hb_period");
	if(item)
	{
		gb28181_cfg->heartbeat_period = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "max_hb_ot");
	if(item)
	{
		gb28181_cfg->max_heartbeat_overtime = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "reg_period");
	if(item)
	{
		gb28181_cfg->register_period = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "expire_time");
	if(item)
	{
		gb28181_cfg->expire_time = item->valueint;
	}

	item = cJSON_GetObjectItem(item_gb28181_params, "adjust_time");
	if(item)
	{
		gb28181_cfg->video_adjust_time = item->valueint;
	}

	return 1;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
回复
{
    “code”: 0,
    “message”: “ok”,
	“messageId”: “这个信息的随机id”
}

*/
std::string CfgRsp(std::string msg_id) 
{
	std::string rsp;
	
	rsp = "{   \"code\": 0,  \"message\": \"ok\", 	\"messageId\": \"" + msg_id + "\"  }";
	
	return rsp;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
URL		webrtc/config/gb28181/{memberId}    //终端id
RequestBody	params	
{	
	“gb28181_params”: {
		“enable“: 1, 								//是否启用
		“istcp“: 1, 	                        	//信令传输协议 1：tcp 0：udp
		“server_type“: "wvp", 				    	//服务器类型 wvp/keda
		“stream_type“: 0,			            	//音视频流类型： 0 音频视频双流 1 仅视频流 2 音视频复合流（单流）
		“local_realm“: “3401000000”,		    	//设备SIP域
		“local_sip_id“:"34010000001180000009",  	//设备SIP ID
		“local_port“: 5060, 						//设备sip信令收发端口
		“local_av_ch“: "34010000001310000009",		//设备媒体通道ID（抽象出的媒体发送设备的编号）
		“peer_ip“: “10.30.2.8”,						//国标服务器IP地址
		“peer_port”:5060,	                    	//国标服务器sip信令收发端口
		“peer_realm”：“4401020049”,					//国标服务器SIP域
		“peer_sip_id”：“44010200492000000001”,		//国标服务器SIP ID
		“user”:"z800-28181-test",					//国标服务器登录用户名
		“passwd”:"admin123",						//国标服务器登录密码
		“hb_period”：20,							//心跳周期，单位秒
		"max_hb_ot":3,								//最大心跳超时次数
		“reg_period”：60，							//注册周期，单位秒
		"expire_time": 3600,						//注册过期时间，单位秒
		"adjust_time": 0							//视频码率自动调整周期，单位秒（默认0为不自动调整）
	}
}
返回
{
    “code”: 0,
    “msg”: “成功”,
	"data": {
	"code":0,
	"message":"ok",
	"data":{}
	}
}
*/
std::string GB28181_CfgEncode(gb_28181_cfg_t *gb28181_cfg)
{
    std::string enable_str         = "\"enable\": " + std::to_string(gb28181_cfg->gb28181_enable) + ", ";
    std::string istcp_str          = "\"istcp\": " + std::to_string(gb28181_cfg->is_tcp) + ", ";
    std::string server_type_str    = "\"server_type\": \"" + std::string((char*)gb28181_cfg->server_type) + "\", ";
    std::string stream_type_str    = "\"stream_type\": " + std::to_string(gb28181_cfg->video_stream_type) + ", ";
    std::string local_realm_str    = "\"local_realm\": \"" + std::string((char*)gb28181_cfg->local_realm) + "\", ";
    std::string local_sip_id_str   = "\"local_sip_id\": \"" + std::string((char*)gb28181_cfg->local_sip_id) + "\", ";
    std::string local_port_str     = "\"local_port\": " + std::to_string(gb28181_cfg->local_port) + ", ";
    std::string local_av_ch_str    = "\"local_av_ch\": \"" + std::string((char*)gb28181_cfg->local_av_channel) + "\", ";
    std::string peer_ip_str        = "\"peer_ip\": \"" + std::string((char*)gb28181_cfg->peer_ip) + "\", ";
    std::string peer_port_str      = "\"peer_port\": " + std::to_string(gb28181_cfg->peer_port) + ", ";
    std::string peer_realm_str     = "\"peer_realm\": \"" + std::string((char*)gb28181_cfg->peer_realm) + "\", ";
    std::string peer_sip_id_str    = "\"peer_sip_id\": \"" + std::string((char*)gb28181_cfg->peer_sip_id) + "\", ";
    std::string user_str           = "\"user\": \"" + std::string((char*)gb28181_cfg->user_name) + "\", ";
    std::string passwd_str         = "\"passwd\": \"" + std::string((char*)gb28181_cfg->passwd) + "\", ";
    std::string hb_period_str      = "\"hb_period\": " + std::to_string(gb28181_cfg->heartbeat_period) + ", ";
    std::string max_hb_ot_str      = "\"max_hb_ot\": " + std::to_string(gb28181_cfg->max_heartbeat_overtime) + ", ";
    std::string reg_period_str     = "\"reg_period\": " + std::to_string(gb28181_cfg->register_period) + ", ";
    std::string expire_time_str    = "\"expire_time\": " + std::to_string(gb28181_cfg->expire_time) + ", ";
    std::string adjust_time_str    = "\"adjust_time\": " + std::to_string(gb28181_cfg->video_adjust_time);

    std::string gb28181_params_str =
        enable_str + istcp_str + server_type_str + stream_type_str +
        local_realm_str + local_sip_id_str + local_port_str + local_av_ch_str +
        peer_ip_str + peer_port_str + peer_realm_str + peer_sip_id_str +
        user_str + passwd_str + hb_period_str + max_hb_ot_str +
        reg_period_str + expire_time_str + adjust_time_str;

	std::string gb28181_str = "{  \"gb28181_params\": {  " + gb28181_params_str + "  }  }";

	return gb28181_str;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
{
    “action”: “gb28181ConfigReplyMsg”,
	“token”: “sdfasdfsd”,
	“messageId”: “随机id标识这个信息”,
	“param”: {
		“eventId”:” deviceConfigMsg命令发来的标志”
		“result”: {
			“code”: 0, // 0 成功
			“message”: “ok”
		},
		“gb28181_params”: {
			“enable“: 1, 								//是否启用
			“istcp“: 1, 	                        	//信令传输协议 1：tcp 0：udp
			“server_type“: "wvp", 				    	//服务器类型 wvp/keda
			“stream_type“: 0,			            	//音视频流类型： 0 音频视频双流 1 仅视频流 2 音视频复合流（单流）
			“local_realm“: “3401000000”,		    	//设备SIP域
			“local_sip_id“:"34010000001180000009",  	//设备SIP ID
			“local_port“: 5060, 						//设备sip信令收发端口
			“local_av_ch“: "34010000001310000009",		//设备媒体通道ID（抽象出的媒体发送设备的编号）
			“peer_ip“: “10.30.2.8”,						//国标服务器IP地址
			“peer_port”:5060,	                    	//国标服务器sip信令收发端口
			“peer_realm”：“4401020049”,					//国标服务器SIP域
			“peer_sip_id”：“44010200492000000001”,		//国标服务器SIP ID
			“user”:"z800-28181-test",					//国标服务器登录用户名
			“passwd”:"admin123",						//国标服务器登录密码
			“hb_period”：20,							//心跳周期，单位秒
			"max_hb_ot":3,								//最大心跳超时次数
			“reg_period”：60，							//注册周期，单位秒
			"expire_time": 3600,						//注册过期时间，单位秒
			"adjust_time": 0							//视频码率自动调整周期，单位秒（默认0为不自动调整）
		}
	}
}

*/
std::string Gb28181_CfgSetRspEncode(std::string eventId, int result, gb_28181_cfg_t *gb28181_cfg)
{
	std::string event_id_str = "\"eventId\": \"" + eventId + "\",   "; 
	
	std::string ok;
	if(result)
	{
		ok = std::string("0,  ")  + "\"message\": \"ok\"   ";
	} else {
		ok = std::string("-1,  ") + "\"message\": \"fail\"   ";
	}
	std::string result_str   = "\"result\": {   \"code\": " + ok +	"   },  ";

	std::string gb28181_params_str = GB28181_CfgEncode(gb28181_cfg);

	std::string params_str = " \"params\": {  " + event_id_str + result_str +  gb28181_params_str + " }  ";

	return params_str;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
URL		webrtc/config/report/{memberId}    //终端id
RequestBody	params	
//哪个配置改变后就上报哪种
{
	“bitRate”: { //码率
		“value”: “500” // 500 Kbps
	}, 
	“resolution”: { //分辨率
		“value”: “640*480” //宽*高
	},
	“fps”: { //帧率
		“value”: “25”
	},
	“osd”: { //osd设置
		“timeStampShow“: true, 						//是否显示时间戳
		“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
		“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
		“timeStampTextSize“: 25,						//时间戳字体大小
		“timeStampMargin“: 25,						//时间戳水印的边距
		“titleShow“: true, 						//是否显示标题文字
		“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
		“titleTextSize“: 25,						//标题字体大小
		“titleMargin“: 25,						//标题水印的边距
		“titleText”:”aa”                        //标题文字
	}
}
返回
{
    “code”: 0,
    “msg”: “成功”,
	"data": {
	"code":0,
	"message":"ok",
	"data":{}
	}
}
*/
std::string CfgReportEncode(av_params_t *av_params)
{
	std::string str = CfgEncode(av_params);
	
	return str;
}
//////////////////////////////////////////////////////////////////////////////////
std::string CfgEncode(av_params_t *av_params)
{
	av_osd_cfg_t       *osd   = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;
	
	std::string fluentfirstmode_str = fluentfisrtEncode();
	std::string bps_str = bitRateEncode(video_cfg->code_rate);
	std::string res_str = resolutionEncode(video_cfg->w, video_cfg->h);
	std::string fps_str = fpsEncode(video_cfg->fps);
	std::string osd_str = osdEncode(osd->time_show, osd->time_format, osd->time_position, osd->time_text_size, osd->time_offset_x,  osd->time_offset_y, 
										osd->title_show, osd->title_position, osd->title_text_size, osd->text_offset_x, osd->text_offset_y, osd->title_text, osd->text_len,
										osd->gnss_show, osd->gnss_position, osd->gnss_text_size, osd->gnss_offset_x, osd->gnss_offset_y,
										osd->bat_sig_show, osd->bat_sig_position, osd->bat_sig_text_size, osd->bat_sig_offset_x, osd->bat_sig_offset_y);
	
	std::string params_str = "{  " + fluentfirstmode_str + bps_str + res_str + fps_str + osd_str + "   }";

	return params_str;
}
//////////////////////////////////////////////////////////////////////////////////
/*
{
    “action”: “deviceConfigReplyMsg”,
	“token”: “sdfasdfsd”,
	“messageId”: “随机id标识这个信息”,
	“param”: {
		“eventId”:” deviceConfigMsg命令发来的标志”
		“result”: {
			“code”: 0, // 0 成功
			“message”: “ok”
		},
		“bitRate”: { //码率
			“value”: “500” // 500 Kbps
		}, 
		“resolution”: { //分辨率
			“value”: “640*480” //宽*高
		},
		“fps”: { //帧率
			“value”: “25”
		},
		“osd”: { //osd设置
			“timeStampShow“: true, 						//是否显示时间戳
			“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
			“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
			“timeStampTextSize“: 25,						//时间戳字体大小
			“timeStampMargin“: 25,						//时间戳水印的边距
			“titleShow“: true, 						//是否显示标题文字
			“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
			“titleTextSize“: 25,						//标题字体大小
			“titleMarginX“: 25,						//标题水印的左右边距
			“titleMarginY“: 25,						//标题水印的上下边距
			“titleText”:”aa”,                       //标题文字
		}
	}
}

*/
std::string CfgSetRspEncode(std::string eventId, int result, av_params_t *av_params)
{
	av_osd_cfg_t       *osd   = &av_params->osd_cfg;
	av_video_cfg_t *video_cfg = &av_params->video_cfg;
	
	std::string event_id_str = "\"eventId\": \"" + eventId + "\",   "; 
	
	std::string ok;
	if(result)
	{
		ok = std::string("0,  ")  + "\"message\": \"ok\"   ";
	} else {
		ok = std::string("-1,  ") + "\"message\": \"fail\"   ";
	}
	std::string result_str   = "\"result\": {   \"code\": " + ok +	"   },  ";
	//	“memberId”:”123”,
		
	std::string bps_str = bitRateEncode(video_cfg->code_rate);
	std::string res_str = resolutionEncode(video_cfg->w, video_cfg->h);
	std::string fps_str = fpsEncode(video_cfg->fps);
	std::string osd_str = osdEncode(osd->time_show, osd->time_format, osd->time_position, osd->time_text_size, osd->time_offset_x, osd->time_offset_y, 
										osd->title_show, osd->title_position, osd->title_text_size, osd->text_offset_x, osd->text_offset_y, osd->title_text, osd->text_len,
										osd->gnss_show, osd->gnss_position, osd->gnss_text_size, osd->gnss_offset_x, osd->gnss_offset_y,
										osd->bat_sig_show, osd->bat_sig_position, osd->bat_sig_text_size, osd->bat_sig_offset_x, osd->bat_sig_offset_y);
	
	std::string params_str = " \"params\": {  " + event_id_str + result_str + bps_str + res_str + fps_str + osd_str + " }  ";

	return params_str;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
请求
{
    “action”: “deviceConfigQueryMsg”,
	“messageId”: “这个信息的随机id”,
	“info”: {
		“eventId”: “标志记录此次事件”;
		“params”: [“bitRate”,” resolution”, “fps”,“osd”] //具体查哪些配置信息
	}
}
*/
int  CfgQueryDecode(cJSON *info, int *fps_flag, int *bps_flag, int *res_flag, int *osd_flag, char *event_id)
{
	cJSON*  item_params, *item_event;
	cJSON   *item_bps, *item_fps, *item_osd, *item_res;
	
	*fps_flag = 0; 
	*bps_flag = 0; 
	*res_flag = 0;
	*osd_flag = 0;
	
	item_params = cJSON_GetObjectItem(info,  "params");
	item_event  = cJSON_GetObjectItem(info, "eventId");
	if(!item_event || !item_params)
	{
		return 0;
	}

	strcpy(event_id, item_event->valuestring);

	item_bps = cJSON_GetObjectItem(info,  "bitRate");
	if(item_bps)
	{
		*bps_flag = 1;
	}
	
	item_fps = cJSON_GetObjectItem(info,  "fps");
	if(item_fps)
	{
		*fps_flag = 1;
	}	
	item_osd = cJSON_GetObjectItem(info,  "osd");
	if(item_osd)
	{
		*osd_flag = 1;
	}
	
	item_res = cJSON_GetObjectItem(info,  "resolution");
	if(item_res)
	{
		*res_flag = 1;
	}
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
/*
Get
URL		/sys/sendComputerMessage
参数 	param JSON
	{
    “id”: “123”, //账号id
	“isVideoAudio”: “0”,  // 0 代表都有 1代表都没有 2代表有视频无音频 3代表无视频有音频
	“mettingStatus”:false, //是否在会议
	“playStatus”:false, //是否在点播别人
	“bePlayStatus”:false, //是否被点播	
	}
*/
std::string CapabilityEncode(std::string user_id, bool audio_cap, bool video_cap)
{
	std::string user_string   = "id=" + user_id + "&";
	std::string cap_str;
	//std::string meeting_status_str = "mettingStatus=false&";
	//std::string play_status_str    = "playStatus=false&"  ;
	std::string beplay_status_str  = "bePlayStatus=true"; // last params
	std::string enc_str;
	
	if(video_cap && audio_cap)
	{
		cap_str = "isVideoAudio=0&";
	}
	else if(video_cap && !audio_cap)
	{
		cap_str = "isVideoAudio=2&";
	}
	else if(!video_cap && audio_cap)
	{
		cap_str = "isVideoAudio=3&";
	}
	else if(!video_cap && !audio_cap)
	{
		cap_str = "isVideoAudio=1&";
	}
	
	//enc_str = "?" + user_string + cap_str + meeting_status_str + play_status_str + beplay_status_str;
	enc_str = "?" + user_string + cap_str + beplay_status_str;

	return enc_str;
}
//////////////////////////////////////////////////////////////////////////////////
/* we not implement it !!!
RequestBody	params	
{
	“bitRate”: { //码率
		“value”: “500” // 500 Kbps
	}, 
	“resolution”: { //分辨率
		“value”: “640*480” //宽*高
	},
	“fps”: { //帧率
		“value”: “25”
	},
	“osd”: { //osd设置
		“timeStampShow“: true, 						//是否显示时间戳
		“timeStampFormat“: “yyyy-MM-dd HH:mm:ss”, 	//时间戳格式
		“timeStampPosition“: 2, 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
		“timeStampTextSize“: 25,						//时间戳字体大小
		“timeStampMargin“: 25,						//时间戳水印的边距
		“titleShow“: true, 						//是否显示标题文字
		“titlePosition“: 2, 						//标题水印位置 1左上 2 右上 3 左下 4 右下
		“titleTextSize“: 25,						//标题字体大小
		“titleMargin“: 25,						//标题水印的边距
		“titleText”:”aa”                        //标题文字
	}
}
返回
{
    “code”: 0,
    “message”: “ok”
}

*/
int gb28181_server_conn_check()
{
    const char *path = "/tmp/gb28181_server_conn";
    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        //MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "[av_cfg] gb28181 conn file open fail: %s", path);
        return 0;
    }
    char buf[16];
    int n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    if (n <= 0)
    {
        MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "[av_cfg] gb28181 conn file empty: %s", path);
        return 0;
    }
    buf[n] = '\0';
    if (buf[0] == '1' || buf[0] == '2')
    {
        MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "[av_cfg] gb28181 conn active: %c", buf[0]);
        return 1;
    }
    MESH_LOG_PRINT(AV_VIDEO_MODULE_NAME, LOG_INFO, "[av_cfg] gb28181 conn inactive: %s", buf);
    return 0;
}
