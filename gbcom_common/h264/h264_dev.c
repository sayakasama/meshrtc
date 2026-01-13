#include "mesh_rtc_inc.h"

#include "h264.h"
#include "rtp_session.h"

/*
H264 file contains frame type:
sps:  about 20 byte
pps:  about 20 byte
I frame
p frame
*/

static void *thread_sender(void *argv);
///////////////////////////////////////////////////////////////////
typedef struct tag_h264_dev {
	int valid;
	
	int  running;
	
	char peer_ip[IP_STR_MAX_LEN];
	int  peer_port;
	int  local_port; 
	char h264_in[FILE_NAME_MAX_LEN]; 
	char h264_out[FILE_NAME_MAX_LEN];
	
	int  fps;
	
	pthread_t tidSender;
	pthread_t tidReceiver;
}h264_dev_t;

h264_dev_t h264_dev;

///////////////////////////////////////////////////////////////////
int   h264_sender_receiver_start(h264_dev_t *h264_dev)
{
	int ret;
	
	ret = pthread_create(&h264_dev->tidSender, NULL, thread_sender, NULL);
	if(ret)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread failed(%d), error:%s(%d).\n",  ret, strerror(errno), errno);
		return MESH_RTC_FAIL;
	}
	
	pthread_detach(h264_dev->tidSender);	
	
	return MESH_RTC_OK;
}

///////////////////////////////////////////////////////////////////
static void *thread_sender(void *argv)
{	
	h264_dev_t *dev = (h264_dev_t *)argv;
	FILE * fp_in  = h264_open_read(dev->h264_in);
#ifdef  H264_DATA_SAVE			
	FILE * fp_out = h264_open_write(dev->h264_out);
	int  recv_session_id, data_len;
#endif	
	int  ret;
	char *buf;
	int  buf_size = 2048;
	int  send_session_id;
	int  send_interval = 1000 / dev->fps; // 1000 ms / fps = interval
	uint64_t last_time = 0, curr_time = sys_time_get();
	
	if(!fp_in)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "open h264 input file failed %s ", dev->h264_in);
#ifdef  H264_DATA_SAVE				
		if(fp_out) h264_close(fp_out);
#endif		
		return NULL;
	}

#ifdef  H264_DATA_SAVE			
	if(!fp_out)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "open h264 output file failed %s ", dev->h264_in);
		if(fp_in) h264_close(fp_in);
		return NULL;
	}
#endif
	
	//
	send_session_id = rtp_session_create(dev->peer_ip, dev->peer_port, NULL, dev->local_port, VIDEO_TYPE_H264_DATA, 1400);
	//
#ifdef  H264_DATA_SAVE			
	recv_session_id = rtp_session_create(dev->peer_ip, dev->peer_port, NULL, dev->local_port, VIDEO_TYPE_H264_DATA, 1400);
#endif
	
	if((send_session_id < 0) 
#ifdef  H264_DATA_SAVE				
		|| (recv_session_id < 0)
#endif		
	)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create rtp session  failed ");		
		h264_close(fp_in);
#ifdef  H264_DATA_SAVE				
		h264_close(fp_out);	
#endif		
		return  NULL;		
	}
	
	buf = (char *)malloc(MAX_H264_FRAME_SIZE);
	while(buf && dev->running)
	{
		if((curr_time - last_time) < send_interval)
		{
			sleep_ms(2);
			continue;
		}
		
		last_time = curr_time;
		// read from file and send
		buf_size = MAX_H264_FRAME_SIZE;
		ret = h264_file_read_frame(fp_in, buf, &buf_size);
		if(ret ==  MESH_RTC_OK)
		{
			ret = rtp_data_send(send_session_id, buf, buf_size);
		}
		
		curr_time = sys_time_get();
		
		//recv and write to file
#ifdef  H264_DATA_SAVE		
		data_len = 0;
		ret = rtp_data_recv(recv_session_id, buf, &data_len, buf_size);
		if(data_len)
		{
			ret = h264_file_write(fp_out, buf, data_len);
		}
#endif		
		
	}

	if(buf)
	{
		free(buf);
	}
	
	h264_close(fp_in);
#ifdef  H264_DATA_SAVE			
	h264_close(fp_out);		
#endif
	
	return  NULL;
}

///////////////////////////////////////////////////////////////////
int h264_dev_open(const char *peer_ip, int peer_port, int local_port, const char *h264_in, const char *h264_out, int fps)
{
	h264_dev_t *dev = &h264_dev;
	int ret;

	rtp_session_init();

	strncpy(dev->peer_ip, peer_ip, IP_STR_MAX_LEN);
	strncpy(dev->h264_in, h264_in, FILE_NAME_MAX_LEN); 
	strncpy(dev->h264_out, h264_out, FILE_NAME_MAX_LEN); 	

	dev->peer_port  = peer_port;
	dev->local_port = local_port; 	
	dev->fps        = fps;
	
	dev->running = 1;
	// create sender: read h264 from file & send data to dest ip/port
	ret = h264_sender_receiver_start(dev);
		
	return ret;
}

///////////////////////////////////////////////////////////////////
int h264_dev_close(void)
{
	h264_dev_t *dev = &h264_dev;
		
	dev->running = 0;
	
	//wait for thread exit
	sleep_ms(50);
	
	return MESH_RTC_OK;
}