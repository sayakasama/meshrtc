#include "mesh_rtc_inc.h"

#include "http_client.h"
#include "circular_queue.h"

///////////////////////////////////////////////////////////////////
typedef struct tag_http_msg_queue {
	int   valid;
	int   running;
	
	int   http_thread_id;
	
	circular_queue_t *queue_in;	
	circular_queue_t *queue_out;
}http_msg_queue_t;	

http_msg_queue_t http_msg_queue_list[HTTP_CLIENT_MAX_NUM];
static int  msg_queue_init_flag = 0;
static int  http_conn_timeout   = 5000; // ms. in mesh network, very bigger delay time is common.

pthread_mutex_t    http_rsp_mutex = PTHREAD_MUTEX_INITIALIZER;
///////////////////////////////////////////////////////////////////////////
void http_msg_queue_init(void);
http_msg_queue_t *http_msg_queue_find(int http_thread_id);
http_msg_queue_t *http_msg_queue_alloc(void);
///////////////////////////////////////////////////////////////////
static void *thread_http(void *argv)
{	
	http_msg_queue_t *http_msg_queue = (http_msg_queue_t *)argv;
	int  msg_size;
	int  ret, rsp_len;
	char *http_rsp, *http_req, *post_data;
	char *p;
	
	post_data= (char *)malloc(HTTP_RSP_MAX_LEN);	
	http_req = (char *)malloc(HTTP_RSP_MAX_LEN);	
	http_rsp = (char *)malloc(HTTP_RSP_MAX_LEN);
	if(!http_rsp || !http_req || !post_data)
	{
		if(http_rsp) free(http_rsp);
		if(http_req) free(http_req);
		if(post_data) free(post_data);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fault!!! alloc %d memory for thread_http", HTTP_RSP_MAX_LEN);
		return NULL;
	}
	
	memset(http_rsp, 0, HTTP_RSP_MAX_LEN);
	memset(http_req, 0, HTTP_RSP_MAX_LEN);
	while(http_msg_queue->running)
	{		
		sleep_ms(10);

		//send msg to peer
		ret     = circular_queue_get(http_msg_queue->queue_out, http_req, &msg_size, HTTP_RSP_MAX_LEN);;
		if(ret !=  MESH_RTC_OK)
		{
			continue;
		}
	
		p = http_req;
		while(*p != ' ')
		{
			if(*p == 0)
			{
				break;
			}
			p++;
		}
		
		// run to the end of http_req or post data str
		rsp_len = 0;
		if(*p == 0) 
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[thread_http]url=%s  \n", http_req);			
			ret = http_client_get(http_req, http_conn_timeout, http_rsp, &rsp_len, HTTP_RSP_MAX_LEN);
		} else {
			strncpy(post_data, p, HTTP_RSP_MAX_LEN);
			*p = 0; // cut string before post data
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[thread_http]url=%s postdata=\n%s \n", http_req, post_data);
			ret = http_client_post(http_req, post_data, http_conn_timeout, http_rsp, &rsp_len, HTTP_RSP_MAX_LEN);
		}
		
		if((ret == MESH_RTC_OK) && rsp_len)
		{
			circular_queue_put(http_msg_queue->queue_in, http_rsp, rsp_len);
			memset(http_rsp, 0, HTTP_RSP_MAX_LEN);
		}
		memset(http_req, 0, HTTP_RSP_MAX_LEN);
	}
	
	circular_queue_delete(http_msg_queue->queue_out);
	circular_queue_delete(http_msg_queue->queue_in);

	free(http_rsp);
	free(http_req);
	free(post_data);
	
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "thread_ipc_msg exit ");
	
	return NULL;
}	
////////////////////////////////////////////////////////////////////////////////////////////
int  http_thread_start(void)
{
	int ret;
	pthread_t tidRtp;
	http_msg_queue_t *http_msg_queue; 
	
	http_msg_queue_init(); // only once init
	
	http_msg_queue = http_msg_queue_alloc();
	
	http_msg_queue->queue_in  = circular_queue_create(HTTP_MSG_MAX_NUM, "http_msg_in");
	http_msg_queue->queue_out = circular_queue_create(HTTP_MSG_MAX_NUM, "http_msg_out");
	if(!http_msg_queue->queue_out || !http_msg_queue->queue_in)
	{
		if(http_msg_queue->queue_in)  circular_queue_delete(http_msg_queue->queue_in);
		if(http_msg_queue->queue_out) circular_queue_delete(http_msg_queue->queue_out);		
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create circular queue failed ");
		return -1;		
	}
	
	http_msg_queue->running = 1;
	ret = pthread_create(&tidRtp, NULL, thread_http, http_msg_queue);
	if(ret)
	{
		circular_queue_delete(http_msg_queue->queue_in);
		circular_queue_delete(http_msg_queue->queue_out);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread_http thread failed(%d), error:%s(%d).\n", ret, strerror(errno), errno);
		return -1;
	}

	pthread_detach(tidRtp);	

	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " start a thread_http .  http_thread_id=%d ", (int)http_msg_queue->http_thread_id);
	
	http_msg_queue->valid = 1;

	return http_msg_queue->http_thread_id;
}

////////////////////////////////////////////////////////////////////////////////////////////
int  http_thread_stop(int http_thread_id)
{
	http_msg_queue_t *msg_queue = http_msg_queue_find(http_thread_id);

	if(!msg_queue)
	{
		return MESH_RTC_FAIL;
	}

	msg_queue->valid   = 0;
	
	msg_queue->running = 0;
	
	return MESH_RTC_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////
int  http_msg_enqueue(int http_thread_id, const char *data)
{
	http_msg_queue_t *msg_queue = http_msg_queue_find(http_thread_id);

	if(!msg_queue)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "invalid msg queue for http_thread_id=%d", http_thread_id);
		return MESH_RTC_FAIL;
	}
	
	int ret = circular_queue_put(msg_queue->queue_out, data, strlen(data));	

	return ret;	
}
////////////////////////////////////////////////////////////////////////////////////////////
int  http_msg_dequeue(int http_thread_id, char *data, int *size, int max_size)
{
	http_msg_queue_t *msg_queue = http_msg_queue_find(http_thread_id);

	*size = 0;
	if(!msg_queue)
	{
		return MESH_RTC_FAIL;
	}
	
	int ret = circular_queue_get(msg_queue->queue_in, data, size, max_size);	
	
	return ret;		
}
////////////////////////////////////////////////////////////////////////////////////////////
http_msg_queue_t *http_msg_queue_find(int http_thread_id)
{
	int loop;
	http_msg_queue_t *msg_queue;
	
	for(loop=0; loop < HTTP_CLIENT_MAX_NUM; loop++)
	{
		msg_queue = &http_msg_queue_list[loop];
		if(msg_queue->valid == 0)
		{
			continue;
		}		
		if(msg_queue->http_thread_id == http_thread_id)
		{
			return msg_queue;
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
http_msg_queue_t *http_msg_queue_alloc(void)
{
	int loop;
	http_msg_queue_t *msg_queue;
	
	for(loop=0; loop < HTTP_CLIENT_MAX_NUM; loop++)
	{
		msg_queue = &http_msg_queue_list[loop];
		if(msg_queue->valid == 0)
		{
			return msg_queue;
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
void http_msg_queue_init(void)
{
	int loop;
	http_msg_queue_t *msg_queue;
	
	if(msg_queue_init_flag)
		return;
	
	msg_queue_init_flag = 1;
	for(loop=0; loop < HTTP_CLIENT_MAX_NUM; loop++)
	{
		msg_queue = &http_msg_queue_list[loop];
		memset(msg_queue, 0, sizeof(http_msg_queue_t));
		msg_queue->http_thread_id = loop;
	}
	
	pthread_mutex_init(&http_rsp_mutex, NULL);
}
