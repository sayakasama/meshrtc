#ifndef  __MESH_AV_CFG_H__
#define  __MESH_AV_CFG_H__

#include "cfg_28181.h"
#ifdef  __cplusplus
extern "C" {
#endif

extern int screen_width_max  ;
extern int screen_height_max ;
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void av_wh_limit_reset(int is_limited);
void av_wh_max_get(int *w, int *h);
void av_wh_max_set(int w, int h);

int av_tmp_rtc_server_save(const char *rtc_server);
int av_tmp_rtc_server_restore(char *rtc_server, int max_size);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int av_audio_cfg_get(av_audio_cfg_t *audio_cfg);
int av_audio_cfg_set(av_audio_cfg_t *audio_cfg);
int av_audio_cfg_hton(av_audio_cfg_t *cfg_in, av_audio_cfg_t *cfg_out);

int av_osd_text_set(char *text, int len);

int av_video_cfg_get(av_params_t *av_params);
int av_video_cfg_set(av_params_t *av_params);
int av_video_cfg_hton(av_params_t *cfg_in, av_params_t *cfg_out);

void av_video_res_get(int *w, int *h);
void av_video_fps_get(int *fps);

int gb28181_server_conn_check();
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef  __cplusplus
}
#endif
std::string CfgRsp(std::string msg_id);

std::string CfgEncode(av_params_t *av_params);

std::string CapabilityEncode(std::string user_name, bool audio_cap, bool video_cap);

std::string CfgReportEncode(av_params_t *av_params);

std::string GB28181_CfgEncode(gb_28181_cfg_t *gb28181_cfg);

int         GB28181_CfgSetDecode(cJSON *info, gb_28181_cfg_t *gb28181_cfg, char *event_id);
std::string Gb28181_CfgSetRspEncode(std::string eventId, int result, gb_28181_cfg_t *gb28181_cfg);

int         CfgSetDecode(cJSON *info, av_params_t *av_params, char *eventId);
std::string CfgSetRspEncode(std::string eventId, int result, av_params_t *av_params);

int         CfgQueryDecode(cJSON *info, int *fps_flag, int *bps_flag, int *res_flag, int *osd_flag, char *event_id); 
std::string CfgQueryRspEncode(std::string msg_id, int fps_flag, int bps_flag, int res_flag, int osd_flag, av_params_t *av_params);

#endif