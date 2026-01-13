#include "mesh_rtc_inc.h"

#include "udp_comm.h"
#include "rtp_session.h"
#include "circular_queue.h"

///////////////////////////////////////////////////////////////////
typedef struct tag_rtp_data_queue {
	int   running;
	
	char  rtp_session_name[64];
	int   rtp_session_id;
	circular_queue_t *queue_in;	
	circular_queue_t *queue_out;
}rtp_data_queue_t;	

rtp_data_queue_t rtp_data_queue_list[RTP_SESSION_MAX_NUM];
static int  data_queue_init = 0;

void rtp_data_queue_init(void);
rtp_data_queue_t *rtp_data_queue_find(int rtp_session_id);
rtp_data_queue_t *rtp_data_queue_alloc(void);
///////////////////////////////////////////////////////////////////
static void *thread_rtp_session(void *argv)
{	
	rtp_data_queue_t *rtp_data_queue = (rtp_data_queue_t *)argv;
	char msg_buf[RTP_PAKCET_MAX_SIZE];
	int  msg_size;
	int  ret;
	int  count_put = 0, count_get=0;
	int  send_session_id = rtp_data_queue->rtp_session_id;
	
	// Just wait for audio device ready. Maybe using a more suitable way ?	
	sleep_ms(500); 
	// clean socket buffer to discard too old data in start stage
	msg_size = 1;
	while(msg_size)
	{
		ret = rtp_data_recv(send_session_id, msg_buf, &msg_size, RTP_PAKCET_MAX_SIZE); 
	}
	
	while(rtp_data_queue->running)
	{
		//send data to local audio dev
		msg_size = 0;
		ret      = circular_queue_get(rtp_data_queue->queue_out, msg_buf, &msg_size, RTP_PAKCET_MAX_SIZE);;
		if(ret ==  MESH_RTC_OK)
		{
			count_get++;
			if(count_get % 1000 == 0)
			{
				//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[%s]circular_queue_get & rtp send : %d \n", rtp_data_queue->rtp_session_name, count_get);
			}
			ret = rtp_data_send(send_session_id, msg_buf, msg_size);
			continue; // get next data
		}
		
		// recv data from local audio dev
		msg_size = 0;
		ret = rtp_data_recv(send_session_id, msg_buf, &msg_size, RTP_PAKCET_MAX_SIZE);
		if(msg_size)
		{
			count_put++;
			if(count_put % 1000 == 0)
			{
				//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[%s]rtp recv & circular_queue_put count=%d msg_size=%d\n", rtp_data_queue->rtp_session_name, count_put, msg_size);
			}
			ret = circular_queue_put(rtp_data_queue->queue_in, msg_buf, msg_size);
			if(ret == MESH_RTC_FAIL)
			{
				if(count_put % 500 == 0)
				MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[%s]circular_queue_put fail: count=%d msg_size=%d rtp_session_id=%d\n", rtp_data_queue->rtp_session_name, count_put, msg_size, rtp_data_queue->rtp_session_id);
			}
			continue;
		}
		
		sleep_ms(1); // sleep and wait if no data 
	}
	
	circular_queue_delete(rtp_data_queue->queue_out);
	circular_queue_delete(rtp_data_queue->queue_in);
	rtp_session_free(send_session_id);
	rtp_data_queue->rtp_session_id = -1;
	
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "thread_rtp_session %s exit ", rtp_data_queue->rtp_session_name);
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_session_thread_start(const char *name, const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type)
{
	return rtp_session_thread_start_ext(name, peer_ip_addr, peer_port, local_ip_addr, local_port, data_type, 1400);
}
////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_session_thread_start_ext(const char *name, const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type, int max_pkt_len)
{
	int ret, data_buf_num;
	pthread_t tidRtp;
	rtp_data_queue_t *rtp_data_queue; 
	char buf[128];
	
	rtp_data_queue_init(); // only once init
	
	rtp_data_queue = rtp_data_queue_alloc(); 
	if(!rtp_data_queue)
	{
		return -1;
	}
	
	strncpy(rtp_data_queue->rtp_session_name, name, 64);
	if(max_pkt_len == 0)
	{
		max_pkt_len = 1400;
	}
	rtp_data_queue->rtp_session_id = rtp_session_create(peer_ip_addr, peer_port, local_ip_addr, local_port, data_type, max_pkt_len);

	if(data_type == VIDEO_TYPE_H264_DATA)
	{
		data_buf_num = RTP_VIDEO_DATA_BUF_NUM;
	} else {	
		data_buf_num = RTP_AUDIO_DATA_BUF_NUM;
	}
	sprintf(buf, "%s_rtp_data_in", name);
	rtp_data_queue->queue_in  = circular_queue_create(data_buf_num, buf);
	sprintf(buf, "%s_rtp_data_out", name);	
	rtp_data_queue->queue_out = circular_queue_create(data_buf_num, buf);
	if(!rtp_data_queue->queue_out || !rtp_data_queue->queue_in)
	{
		if(rtp_data_queue->queue_in)  circular_queue_delete(rtp_data_queue->queue_in);
		if(rtp_data_queue->queue_out) circular_queue_delete(rtp_data_queue->queue_out);		
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create circular queue failed ");
		return -1;		
	}
	
	rtp_data_queue->running = 1;
	ret = pthread_create(&tidRtp, NULL, thread_rtp_session, rtp_data_queue);
	if(ret)
	{
		circular_queue_delete(rtp_data_queue->queue_in);
		circular_queue_delete(rtp_data_queue->queue_out);
		rtp_session_free(rtp_data_queue->rtp_session_id);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create rtp_session_thread_start thread failed(%d), error:%s(%d).\n",  ret, strerror(errno), errno);
		return -1;
	}

	pthread_detach(tidRtp);	

	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " %s: rtp session thread starting... peer : %s %d local : %s %d -- rtp_session_id=%d ", name, peer_ip_addr, peer_port, local_ip_addr, local_port, rtp_data_queue->rtp_session_id);
	
	return rtp_data_queue->rtp_session_id;
}

////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_session_thread_stop(int rtp_session_id)
{
	rtp_data_queue_t *data_queue = rtp_data_queue_find(rtp_session_id);

	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	
	data_queue->running = 0;
	return MESH_RTC_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_session_data_enqueue(int rtp_session_id, char *data, int size)
{
	rtp_data_queue_t *data_queue = rtp_data_queue_find(rtp_session_id);
	int ret, len, offset;
	
	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	
	if(size <= RTP_PAYLOAD_MAX_SIZE)
	{
		ret = circular_queue_put(data_queue->queue_out, data, size);	
		return ret;
	}
	
	len    = RTP_PAYLOAD_MAX_SIZE;
	offset = 0;
	while((offset + len) < size)
	{
		ret     = circular_queue_put(data_queue->queue_out, data + offset, len);
		offset += len;
	}
	//last frag
	ret = circular_queue_put(data_queue->queue_out, data + offset, size - offset);
	
	return ret;	
}
////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_session_data_dequeue(int rtp_session_id, char *data, int *size, int max_size)
{
	int ret;
	
	rtp_data_queue_t *data_queue = rtp_data_queue_find(rtp_session_id);

	*size = 0;
	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	
	ret = circular_queue_get(data_queue->queue_in, data, size, max_size);	
	
	return ret;		
}
////////////////////////////////////////////////////////////////////////////////////////////
rtp_data_queue_t *rtp_data_queue_find(int rtp_session_id)
{
	int loop;
	rtp_data_queue_t *data_queue;
	
	for(loop=0; loop < RTP_SESSION_MAX_NUM; loop++)
	{
		data_queue = &rtp_data_queue_list[loop];
		if(data_queue->rtp_session_id == rtp_session_id)
		{
			return data_queue;
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
rtp_data_queue_t *rtp_data_queue_alloc(void)
{
	int loop;
	rtp_data_queue_t *data_queue;
	
	for(loop=0; loop < RTP_SESSION_MAX_NUM; loop++)
	{
		data_queue = &rtp_data_queue_list[loop];
		if(data_queue->rtp_session_id == -1)
		{
			return data_queue;
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
void rtp_data_queue_init(void)
{
	int loop;
	rtp_data_queue_t *data_queue;
	
	if(data_queue_init)
		return;
	
	data_queue_init = 1;
	for(loop=0; loop < RTP_SESSION_MAX_NUM; loop++)
	{
		data_queue = &rtp_data_queue_list[loop];
		memset(data_queue, 0, sizeof(rtp_data_queue_t));
		data_queue->rtp_session_id = -1;
	}
}
