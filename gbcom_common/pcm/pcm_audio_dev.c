#include "mesh_rtc_inc.h"

#include "pcm.h"
#include "rtp_session.h"

static void *thread_sender(void *argv);
///////////////////////////////////////////////////////////////////
typedef struct tag_pcm_audio_dev {
	int valid;
	
	int  running;
	
	char peer_ip[IP_STR_MAX_LEN];
	int  peer_port;
	int  local_port; 
	char pcm_in[FILE_NAME_MAX_LEN]; 
	char pcm_out[FILE_NAME_MAX_LEN];
	
	int  sample;
	int  data_bits;
	int  ms;
	
	pthread_t tidSender;
	pthread_t tidReceiver;
}pcm_audio_dev_t;

pcm_audio_dev_t pcm_audio_dev;

///////////////////////////////////////////////////////////////////
int   pcm_sender_receiver_start(pcm_audio_dev_t *pcm_audio_dev)
{
	int ret;
	
	ret = pthread_create(&pcm_audio_dev->tidSender, NULL, thread_sender, NULL);
	if(ret)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread failed(%d), error:%s(%d).\n",  ret, strerror(errno), errno);
		return MESH_RTC_FAIL;
	}
	
	pthread_detach(pcm_audio_dev->tidSender);	
	
	return MESH_RTC_OK;
}

///////////////////////////////////////////////////////////////////
int   pcm_receiver_start(pcm_audio_dev_t *pcm_audio_dev)
{
	int ret = 1;
	
	//ret = pthread_create(&pcm_audio_dev->tidReceiver, NULL, thread_receiver, NULL);
	if(ret)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread failed(%d), error:%s(%d).\n",  ret, strerror(errno), errno);
		return MESH_RTC_FAIL;
	}
	
	pthread_detach(pcm_audio_dev->tidReceiver);	
	
	return MESH_RTC_OK;
}
///////////////////////////////////////////////////////////////////
static void *thread_sender(void *argv)
{	
	pcm_audio_dev_t *audio_dev = (pcm_audio_dev_t *)argv;
	FILE * fp_in  = pcm_open_read(audio_dev->pcm_in);
	FILE * fp_out = pcm_open_write(audio_dev->pcm_out);
	int  ret, data_len;
	char buf[2048];
	int  buf_size = 2048;
	int  send_session_id;
	int  recv_session_id;
	
	if(!fp_in)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "open pcm input file failed %s ", audio_dev->pcm_in);
		if(fp_out) pcm_close(fp_out);
		return NULL;
	}
	if(!fp_out)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "open pcm output file failed %s ", audio_dev->pcm_in);
		if(fp_in) pcm_close(fp_in);
		return NULL;
	}
	
	//
	send_session_id = rtp_session_create(audio_dev->peer_ip, audio_dev->peer_port, NULL, audio_dev->local_port, AUDIO_TYPE_PCMA_DATA, 1400);
	//
	recv_session_id = rtp_session_create(audio_dev->peer_ip, audio_dev->peer_port, NULL, audio_dev->local_port, AUDIO_TYPE_PCMA_DATA, 1400);
	
	if((send_session_id < 0) || (recv_session_id < 0))
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create rtp session  failed ");		
		pcm_close(fp_in);
		pcm_close(fp_out);		
		return  NULL;		
	}
	
	while(audio_dev->running)
	{
		// read from file and send
		ret = pcm_file_read(fp_in, audio_dev->sample, audio_dev->data_bits, buf, buf_size, 1);
		if(ret > 0)
		{
			ret = rtp_data_send(send_session_id, buf, ret);
		}
		
		//recv and write to file
		data_len = 0;
		ret = rtp_data_recv(recv_session_id, buf, &data_len, buf_size);
		if(data_len)
		{
			ret = pcm_file_write(fp_out, buf, data_len);
		}
		
		// not accurate !!!
		sleep_ms(audio_dev->ms - 1);
	}
	
	pcm_close(fp_in);
	pcm_close(fp_out);		
	
	return  NULL;
}

///////////////////////////////////////////////////////////////////
int pcm_audio_dev_open(const char *peer_ip, int peer_port, int local_port, const char *pcm_in, const char *pcm_out, int sample, int data_bits, int ms)
{
	pcm_audio_dev_t *audio_dev = &pcm_audio_dev;
	int ret;

	rtp_session_init();

	strncpy(audio_dev->peer_ip, peer_ip, IP_STR_MAX_LEN);
	strncpy(audio_dev->pcm_in, pcm_in, FILE_NAME_MAX_LEN); 
	strncpy(audio_dev->pcm_out, pcm_out, FILE_NAME_MAX_LEN); 	

	audio_dev->peer_port  = peer_port;
	audio_dev->local_port = local_port; 	
	audio_dev->sample     = sample;
	audio_dev->ms         = ms;	
	audio_dev->data_bits  = data_bits;
	
	audio_dev->running = 1;
	// create sender: read pcm from file & send data to dest ip/port
	ret = pcm_sender_receiver_start(audio_dev);
	
	// create receiver: receiver pcm from ip/port & write to file	
	//ret = pcm_receiver_start(audio_dev);	
	
	return ret;
}

///////////////////////////////////////////////////////////////////
int pcm_audio_dev_close(void)
{
	pcm_audio_dev_t *audio_dev = &pcm_audio_dev;
		
	audio_dev->running = 0;
	
	//wait for thread exit
	sleep_ms(audio_dev->ms * 2);
	
	return MESH_RTC_OK;
}