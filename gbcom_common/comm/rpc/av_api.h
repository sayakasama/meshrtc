#ifndef __RPC_GSOAP_AV_API_H__
#define __RPC_GSOAP_AV_API_H__

#ifdef  __cplusplus
extern "C" {
#endif
///////////////////////////////////////////////////////////////
void av_api_init(const char *server);

int hello( char *cmd_req, char *cmd_rsp);

int audio_sample_get( int *sample);
int audio_sample_set( int sample);
int audio_button_status_get( int *button_status);

int video_interval_set( int interval);
int video_interval_get( int *interval);

int video_fps_get( int *fps);
int video_fps_set( int fps);

int video_wh_set( int w, int h);
int video_wh_get( int *w, int *h);

int video_bitrate_set(int bitrate_max, int bitrate_min);
int video_bitrate_get(int *bitrate_max, int *bitrate_min);

///////////////////////////////////////////////////////////////
int video_cfg_set(char*  cfg_data, int len);
int video_cfg_get(char*  cfg_data);

int audio_cfg_set(char*  cfg_data, int len);
int audio_cfg_get(char*  cfg_data);

int osd_text_set(char*  photo_data, int len);
///////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif
