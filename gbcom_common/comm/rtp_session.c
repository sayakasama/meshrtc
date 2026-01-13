#include "mesh_rtc_inc.h"

#include "udp_comm.h"
#include "rtp_session.h"

////////////////////////////////////////////////////////////////////////////////////////////
pthread_mutex_t    rtp_mutex = PTHREAD_MUTEX_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////////////////
rtp_session_t  rtp_session_list[RTP_SESSION_MAX_NUM] = {0};
////////////////////////////////////////////////////////////////////////////////////////////
void rtp_session_init(void)
{
	int loop;
	rtp_session_t * rtp_session;
	
	for(loop=0; loop < RTP_SESSION_MAX_NUM; loop++)
	{
		rtp_session = &rtp_session_list[loop];		
		memset(rtp_session, 0, sizeof(rtp_session_t));
		rtp_session->id = loop;
	}
	
	pthread_mutex_init(&rtp_mutex, NULL);
}

////////////////////////////////////////////////////////////////////////////////////////////
rtp_session_t  *rtp_session_alloc(void)
{
	int loop;
	rtp_session_t * rtp_session;
	
	pthread_mutex_lock(&rtp_mutex);				
	for(loop=0; loop < RTP_SESSION_MAX_NUM; loop++)
	{
		rtp_session = &rtp_session_list[loop];		
		if(0 == rtp_session->valid)
		{
			rtp_session->id    = loop;
			rtp_session->valid = 1;
			pthread_mutex_unlock(&rtp_mutex);				
			return rtp_session;
		}
	}
	pthread_mutex_unlock(&rtp_mutex);				
	
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp_session_alloc failed");
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
void rtp_session_free(int session_id)
{
	rtp_session_t * rtp_session;
	
	if(session_id >= RTP_SESSION_MAX_NUM)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp_session_free failed. session_id=%d > RTP_SESSION_MAX_NUM %d", session_id, RTP_SESSION_MAX_NUM);
		return ;
	}

	pthread_mutex_lock(&rtp_mutex);				
	rtp_session = &rtp_session_list[session_id];
	if(0 == rtp_session->valid)
	{
		pthread_mutex_unlock(&rtp_mutex);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp_session_free failed. session_id=%d is free", session_id);
		return ;
	}
	
	udp_close(rtp_session->sockfd);
	
	memset(rtp_session, 0, sizeof(rtp_session_t));
	pthread_mutex_unlock(&rtp_mutex);
}
////////////////////////////////////////////////////////////////////////////////////////////
rtp_session_t  *rtp_session_find(int session_id)
{
	rtp_session_t * rtp_session;
	
	if(session_id >= RTP_SESSION_MAX_NUM)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp_session_find failed. session_id=%d > RTP_SESSION_MAX_NUM =%d ", session_id, RTP_SESSION_MAX_NUM);		
		return NULL;
	}

	// Notice: we not use mutex to protect list get!!! Because this function is called in many thread, mutex will
	// cause a very huge performce degrade.
	// In worst case , an invalid session is returned and data sending will fail. But it's ok because session has been released
	//pthread_mutex_lock(&rtp_mutex);
	rtp_session = &rtp_session_list[session_id];		
	if(0 == rtp_session->valid)
	{
		//pthread_mutex_unlock(&rtp_mutex);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp_session_find failed. session_id=%d is invalid", session_id);				
		return NULL;
	}
	
	//pthread_mutex_unlock(&rtp_mutex);				
	return rtp_session;			
}
////////////////////////////////////////////////////////////////////////////////////////////
int rtp_session_create(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, int data_type, int max_pkt_len)
{
	rtp_session_t  *rtp_session = rtp_session_alloc();
	
	if(!rtp_session)
	{
		return -1;
	}
	
	rtp_session->pt_error_num = 0;
	rtp_session->max_pkt_len  = max_pkt_len;
	rtp_session->peer_port    = peer_port; 
	rtp_session->local_port   = local_port;
	rtp_session->data_type    = data_type;
	rtp_session->sockfd       = udp_sock_create(peer_ip_addr, peer_port, local_ip_addr, local_port, &rtp_session->peer_udp_addr);
	if(rtp_session->sockfd > 0)
	{
		return rtp_session->id;
	}
	
	return -1;
}
////////////////////////////////////////////////////////////////////////////////////////////
int rtp_data_send(int rtp_session_id, char *data, int len)
{
	int ret, send_len, pkt_len, send_bytes;
	char local_buf[RTP_PAKCET_MAX_SIZE];
	rtp_head_t     *rtp_head    = (rtp_head_t *)local_buf;;	
	rtp_session_t  *rtp_session = rtp_session_find(rtp_session_id);
	
	if(!rtp_session)
	{
		return 0;
	}
	
	pkt_len    = rtp_session->max_pkt_len;
	send_len   = 0;
	send_bytes = 0;
	ret        = 0;
	while(send_len < len)
	{
		if((len - send_len) >= pkt_len)
		{
			send_bytes = pkt_len;
		} else {
			send_bytes = len - send_len; // last frag data 
		}
		
		//data head
		rtp_head->v  = MESH_RTP_VERSION;	
		rtp_head->pt = rtp_session->data_type;	
		rtp_head->sn = rtp_session->sn++;	
		rtp_head->timestamp = 0; // todo. ms * 90000
	
		// data body
		memcpy(local_buf + sizeof(rtp_head_t), data + send_len, send_bytes);
		send_len += send_bytes;
		if(send_len == len)
		{
			rtp_head->m = 1; // last fragment of a video frame	
		}
		
		// data head + body
		ret = udp_send(rtp_session->sockfd, local_buf, send_bytes + sizeof(rtp_head_t), &rtp_session->peer_udp_addr);
		if(ret <= 0)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "send error: rtp_session_id=%d", rtp_session_id);
		}
	}
	
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_data_recv(int rtp_session_id, char *data, int *data_len, int max_size)
{
	rtp_session_t  *rtp_session = rtp_session_find(rtp_session_id);
	rtp_head_t     *rtp_head;	
	char buf[2048]; // video & audio frame not bigger than 1500

	if(!rtp_session)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "rtp sesson is not valid %d ", rtp_session_id);				
		return 0;
	}
	
	*data_len = udp_recv(rtp_session->sockfd, buf, max_size, &rtp_session->peer_udp_addr);
	if(*data_len <= sizeof(rtp_head_t))
	{
		*data_len = 0;
		return 0; // invalid rtp data
	}
	
	rtp_head = (rtp_head_t *)buf;
	if(MESH_RTP_VERSION != rtp_head->v)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "rtp version is not %d !!! rtp_head->v=%d ", MESH_RTP_VERSION, rtp_head->v);		
		return 0;
	}
	
	if(rtp_head->pt != rtp_session->data_type)
	{
		*data_len = 0;
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "data type error!!! rtp_head->pt=%d rtp_session->data_type=%d local_port=%d peer_port=%d pt_error_num=%d", \
					rtp_head->pt, rtp_session->data_type, rtp_session->local_port, rtp_session->peer_port, rtp_session->pt_error_num);
		return 0;
	}

	*data_len -= sizeof(rtp_head_t);
	memcpy(data, buf + sizeof(rtp_head_t), *data_len);
	return *data_len;	
}