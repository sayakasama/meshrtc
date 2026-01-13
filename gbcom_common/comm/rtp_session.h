#ifndef __RTP_SESSION_H__
#define __RTP_SESSION_H__


#ifdef  __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////
typedef struct tag_rtp_head {
  unsigned char   cc:4; // 0, no CSCR  
  unsigned char   x:1; // 0, no ext field  
  unsigned char   p:1; // 0, no extra payload
  unsigned char   v:2; // version: 3

  unsigned char   pt:7; // payload type  
  unsigned char   m:1;  // 0, video frame end
  
  unsigned short  sn;
  unsigned int    timestamp;
  unsigned int    ssrc;
}__attribute__((packed)) rtp_head_t; // 12 byte

#define MESH_RTP_VERSION 3

#define AUDIO_TYPE_PCMU_DATA  0
#define AUDIO_TYPE_PCMA_DATA  8
#define VIDEO_TYPE_H264_DATA  96
#define VIDEO_TYPE_H265_DATA  97
#define AUDIO_TYPE_FOWARD_DATA  127

#define PTT_AUDIO_CMD   126
#define PTT_AUDIO_DATA  127

#define RTP_PAKCET_MAX_SIZE 2048
#define EXTRA_HEAD_LEN      (20+8+16) // ip/udp/rtp

#define RTP_AUDIO_DATA_BUF_NUM  8 // 10ms frame max delay 10ms*8=80ms
#define RTP_VIDEO_DATA_BUF_NUM  (512*3) // 1536*1400=2.1M byte for max video frame

#define  RTP_PAYLOAD_MAX_SIZE  1300 // eth: 14; ip: 20 ; udp: 8; rtp: 20; tls: 16 & ps head for video
/////////////////////////////////////////
typedef struct tag_rtp_session {
	int    valid;
	int    id;
	
	int    sockfd;
	int    data_type;
	int    max_pkt_len;
	struct sockaddr_in peer_udp_addr;
	
	int    peer_port; 
	int    local_port;
	// statastic
	int    sn;
	int    recv_pkt_lost;
	
	int    pt_error_num;
}rtp_session_t;

#define RTP_SESSION_MAX_NUM  6
/////////////////////////////////////////////////////////
void rtp_session_init(void);

int  rtp_session_create(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type, int max_pkt_len);
void rtp_session_free(int session_id);

int  rtp_data_send(int rtp_session_id, char *data, int len);
int  rtp_data_recv(int rtp_session_id, char *data, int *len, int max_size);

/////////////////////////////////////////////////////////
int  rtp_session_thread_start_ext(const char *name, const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type, int max_pkt_len);
int  rtp_session_thread_start(const char *name, const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type);
int  rtp_session_thread_stop(int rtp_session_id);

int  rtp_session_data_dequeue(int rtp_session_id, char *data, int *size, int max_size);
int  rtp_session_data_enqueue(int rtp_session_id, char *data, int size);
/////////////////////////////////////////////////////////


#ifdef  __cplusplus
}
#endif


#endif
