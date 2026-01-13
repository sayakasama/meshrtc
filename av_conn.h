#ifndef  __MESH_AV_CONN_H__
#define  __MESH_AV_CONN_H__


#ifdef  __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
void av_module_conn_check(int  dev_type);
void av_audio_encoder_restart();

int  av_video_conn_ok();

void av_server_status_update(int  dev_type, int status);

int av_internal_ip_get(int  dev_type, unsigned char *local_ip);
int av_external_ip_get(int  dev_type, char *ip);
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void av_video_cfg_dump(av_params_t *av_params);
void av_audio_cfg_dump(av_audio_cfg_t *audio_cfg);

#ifdef  __cplusplus
}
#endif


#endif