#ifndef __VIDEO_DATA_RECV_H__
#define __VIDEO_DATA_RECV_H__

#ifdef  __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////
int  encoder_frame_rate_set(int fps);
int  encoder_bps_set(int bps);
int  encoder_force_idr();

int  video_osd_cfg_update(av_osd_cfg_t *osd);
int  encoder_cfg_update(int codec, int w, int h, int fps, int gop, int bps);

void encoder_wh_set(int w, int h);
void encoder_wh_get(int *w, int *h);
////////////////////////////////////////////////////////////////////////////////////////////////////////
int  encoded_data_get(char *data, int *len, int *have_encoded_num);
int  encoded_data_ready();
int  encoded_init_ok();
int  encoded_data_trigger();
void encoded_data_clean();

int  video_recv_start(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port);
void video_recv_stop(int reason);
////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif