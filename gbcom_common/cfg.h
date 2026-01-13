#ifndef  __MESH_CFG_H__
#define  __MESH_CFG_H__


#ifdef  __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////
int get_property(const char *key, char * desValue, int len);
int set_property(const char *key, const char * scValue);

int cfg_ptt_multicast_port_get(int *port);
int cfg_ptt_multicast_port_set(int port);
int cfg_ptt_unicast_port_get(int *port);
int cfg_ptt_unicast_port_set(int port);
int cfg_ptt_priority_set(int priority);
int cfg_ptt_priority_get(int *priority);

int audio_samples_set(int samples);
int audio_samples_get(int *samples);
//////////////////////////////////////////////////////////////////
int cfg_nickname_get(char *name, int max_size);
int cfg_name_get(char *name, int max_size);

int cfg_dev_sn_get(char *dev_sn, int max_size);
int cfg_dev_type_get(int *dev_type);
int cfg_video_id_get(char *video_id, int max_size);
int cfg_video_enable_get(int *video_enable);

int cfg_rtc_server_get(char *rtc_server);

int cfg_samples_set(int codec);
int cfg_samples_get(int *codec);

int cfg_aec_set(int codec);
int cfg_aec_get(int *codec);

int cfg_volume_set(int codec);
int cfg_volume_get(int *codec);

int cfg_audio_chan_get(char *audio_chan);
int cfg_audio_chan_set(const char* audio_chan);

int cfg_qp_set(int qp);
int cfg_qp_get(int *qp);

int cfg_rcmode_set(int rcmode);
int cfg_rcmode_get(int *rcmode);

int outline_qp_calculate(int w,int h,int fps,int code_rate);
int qp_calculate(av_video_cfg_t *av_cfg);

int cfg_codec_set(int codec);
int cfg_codec_get(int *codec);

int cfg_gop_get(int *gop);
int cfg_gop_set(int gop);

int cfg_fps_get(int *fps);
int cfg_fps_set(int fps);

int cfg_bps_get(int *bps);
int cfg_bps_set(int bps);

int cfg_resolution_get(int *w, int *h);
int cfg_resolution_set(int w, int h);

int cfg_osd_set(int timeStampShow, const char *timeStampFormat, int timeStampPosition, int timeStampTextSize, int timeStampMarginX, int timeStampMarginY, 
				int titleShow, int titlePosition, int titleTextSize, int titleMarginX, int titleMarginY, const char *titleText, int textLen,
				int gnssStampShow, int gnssStampPosition, int gnssStampTextSize, int gnssStampMarginX, int gnssStampMarginY,
				int batSigShow, int batSigPosition, int batSigTextSize, int batSigMarginX, int batSigMarginY);

int cfg_osd_get(int *timeStampShow, char *timeStampFormat, int *timeStampPosition, int *timeStampTextSize, int *timeStampMarginX, int *timeStampMarginY, 
				int *titleShow, int *titlePosition, int *titleTextSize, int *titleMarginX, int *titleMarginY, char *titleText, int *textLen,
				int *gnssStampShow, int *gnssStampPosition, int *gnssStampTextSize, int *gnssStampMarginX, int *gnssStampMarginY,
				int *batSigShow, int *batSigPosition, int *batSigTextSize, int *batSigMarginX, int *batSigMarginY);
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif