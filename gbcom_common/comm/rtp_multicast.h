#ifndef __RTP_MULTICAST_H__
#define __RTP_MULTICAST_H__

#ifdef  __cplusplus
extern "C" {
#endif


int  rtp_multicast_init(const char *group_ip, int group_port, const char *interface);
void rtp_multicast_release(void);

int  rtp_multicast_recv(char *data, int *data_len, int *data_type, int max_size, int *sn, int *timestamp);
int  rtp_multicast_send(const char *data, int len, int data_type);


#ifdef  __cplusplus
}
#endif


#endif //


