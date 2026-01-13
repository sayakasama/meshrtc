#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "websocket_client.h"
#include "circular_queue.h"

///////////////////////////////////////////////////////////////////
typedef struct tag_websocket_data_queue {
	int   running;
	int   finish;
	
	char  url[URL_MAX_LEN];
	int   websocket_session_id;
	void  *websocket_session_data;
	circular_queue_t *queue_in;	
	circular_queue_t *queue_out;
	pthread_mutex_t   queue_mutex;
}websocket_data_queue_t;	

websocket_data_queue_t websocket_data_queue_list[WS_SESSION_MAX_NUM] = {0};
static int  ws_data_queue_init = 0;

///////////////////////////////////////////////////////////////////////////
void websocket_data_queue_init(void);
websocket_data_queue_t *websocket_data_queue_find(int websocket_session_id);
///////////////////////////////////////////////////////////////////
static void *thread_websocket_session(void *argv)
{	
	websocket_data_queue_t *websocket_data_queue = (websocket_data_queue_t *)argv;
	char msg_buf[WS_MSG_MAX_SIZE];
	int  msg_size;
	int  ret, type, reason = 0;
	
	while(websocket_data_queue->running)
	{
		if(websocket_data_queue->websocket_session_data == NULL)
		{
			// connect web server
			websocket_data_queue->websocket_session_data = websocket_conn_init(websocket_data_queue->url, 10000);
		}
		
		if(websocket_data_queue->websocket_session_data == NULL)
		{
			sleep_ms(1000); // re-connect web server
			continue;
		}
		
		sleep_ms(10);

		//send msg to peer
		memset(msg_buf, 0, WS_MSG_MAX_SIZE);
		pthread_mutex_lock(&websocket_data_queue->queue_mutex);
		ret     = circular_queue_get(websocket_data_queue->queue_out, msg_buf, &msg_size, WS_MSG_MAX_SIZE);
		pthread_mutex_unlock(&websocket_data_queue->queue_mutex);
		if(ret ==  MESH_RTC_OK)
		{
			ret = websocket_send_text(websocket_data_queue->websocket_session_data, msg_buf);
			if(ret == MESH_RTC_FAIL) // send data error and reconnect
			{
				websocket_close(websocket_data_queue->websocket_session_data);
				websocket_data_queue->websocket_session_data = NULL;
				websocket_data_queue->running = 0;
				reason = 1;
				break;
			}
		}
		
		// recv msg from peer				
		ret    = websocket_recv_text(websocket_data_queue->websocket_session_data, msg_buf, WS_MSG_MAX_SIZE, &type);
		if(ret > 0)
		{
			pthread_mutex_lock(&websocket_data_queue->queue_mutex);
			circular_queue_put(websocket_data_queue->queue_in, msg_buf, strlen(msg_buf));
			pthread_mutex_unlock(&websocket_data_queue->queue_mutex);			
		}
		else if(ret == MESH_RTC_FAIL) 
		{
			// peer closed websocket
			websocket_close(websocket_data_queue->websocket_session_data);
			websocket_data_queue->websocket_session_data = NULL;
			websocket_data_queue->running = 0;
			reason = 2;
			break;
		}		
	}
	
	pthread_mutex_lock(&websocket_data_queue->queue_mutex);	
	circular_queue_delete(websocket_data_queue->queue_out);
	circular_queue_delete(websocket_data_queue->queue_in);
	websocket_data_queue->queue_out = NULL;
	websocket_data_queue->queue_in  = NULL;
	pthread_mutex_unlock(&websocket_data_queue->queue_mutex);
	
	if(websocket_data_queue->websocket_session_data)
	{
		websocket_close(websocket_data_queue->websocket_session_data);
	}
	
	websocket_data_queue->finish  = 1;
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "thread_websocket_session exit reason=%d", reason);
	
	return NULL;
}	
////////////////////////////////////////////////////////////////////////////////////////////
int  websocket_session_thread_start(const char *url)
{
	int ret, loop;
	pthread_t tidRtp;
	websocket_data_queue_t *websocket_data_queue, *data_queue = NULL; 
	
	websocket_data_queue_init(); // only once init

	for(loop=0; loop < WS_SESSION_MAX_NUM; loop++)
	{
		data_queue = &websocket_data_queue_list[loop];
		if(!data_queue->running && data_queue->finish)
		{
			websocket_data_queue = data_queue; // find an idle queue
			break;
		}
	}
	if(!websocket_data_queue)
	{
		return -1;
	}
	
	strcpy(websocket_data_queue->url, url);
	
	websocket_data_queue->queue_in  = circular_queue_create(WS_MSG_BUF_NUM, "websocket_in");
	websocket_data_queue->queue_out = circular_queue_create(WS_MSG_BUF_NUM, "websocket_out");
	if(!websocket_data_queue->queue_out || !websocket_data_queue->queue_in)
	{
		if(websocket_data_queue->queue_in)  circular_queue_delete(websocket_data_queue->queue_in);
		if(websocket_data_queue->queue_out) circular_queue_delete(websocket_data_queue->queue_out);		
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create circular queue failed ");
		return -1;		
	}
	
	websocket_data_queue->finish  = 0;
	websocket_data_queue->running = 1;
	ret = pthread_create(&tidRtp, NULL, thread_websocket_session, websocket_data_queue);
	if(ret)
	{
		circular_queue_delete(websocket_data_queue->queue_in);
		circular_queue_delete(websocket_data_queue->queue_out);
		websocket_data_queue->finish  = 1;
		websocket_data_queue->running = 0;
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread_websocket_session thread for %s failed(%d), error:%s(%d).\n", url,  ret, strerror(errno), errno);
		return -1;
	}

	pthread_detach(tidRtp);	

	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " start a websocket_session_thread_start. url :\n %s  \n websocket_session_id=%d \n", url, (int)websocket_data_queue->websocket_session_id);
	
	return websocket_data_queue->websocket_session_id;
}

////////////////////////////////////////////////////////////////////////////////////////////
int  websocket_session_thread_stop(int websocket_session_id)
{
	websocket_data_queue_t *data_queue = websocket_data_queue_find(websocket_session_id);
	int count = 50;
	
	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " websocket_session_thread_stop. url :\n %s  \n websocket_session_id=%d \n", data_queue->url, (int)data_queue->websocket_session_id);
	
	data_queue->running = 0;
	while(!data_queue->finish && count--)
	{
		sleep_ms(10);
	}
	
	return MESH_RTC_OK;
}
/////////////////////////////////////////////////////////////////////////////////////////////
int  websocket_session_data_enqueue(int websocket_session_id, const char *data, int size)
{
	websocket_data_queue_t *data_queue = websocket_data_queue_find(websocket_session_id);

	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	if(data_queue->running == 0)
	{
		return MESH_RTC_FAIL;
	}
	
	pthread_mutex_lock(&data_queue->queue_mutex);	
	int ret = circular_queue_put(data_queue->queue_out, data, size);	
	pthread_mutex_unlock(&data_queue->queue_mutex);
	
	return ret;	
}
////////////////////////////////////////////////////////////////////////////////////////////
int  websocket_session_data_dequeue(int websocket_session_id, char *data, int *size, int max_size)
{
	websocket_data_queue_t *data_queue = websocket_data_queue_find(websocket_session_id);

	if(!data_queue)
	{
		return MESH_RTC_FAIL;
	}
	if(data_queue->running == 0)
	{
		return MESH_RTC_FAIL;
	}
	
	pthread_mutex_lock(&data_queue->queue_mutex);
	int ret = circular_queue_get(data_queue->queue_in, data, size, max_size);	
	pthread_mutex_unlock(&data_queue->queue_mutex);
	
	return ret;		
}
////////////////////////////////////////////////////////////////////////////////////////////
websocket_data_queue_t *websocket_data_queue_find(int websocket_session_id)
{
	int loop;
	websocket_data_queue_t *data_queue;
	
	for(loop=0; loop < WS_SESSION_MAX_NUM; loop++)
	{
		data_queue = &websocket_data_queue_list[loop];
		if(data_queue->websocket_session_id == websocket_session_id)
		{
			return data_queue;
		}
	}
	
	return NULL;
}
////////////////////////////////////////////////////////////////////////////////////////////
void websocket_data_queue_init(void)
{
	int loop;
	websocket_data_queue_t *data_queue;
	
	if(ws_data_queue_init)
		return;
	
	for(loop=0; loop < WS_SESSION_MAX_NUM; loop++)
	{
		data_queue = &websocket_data_queue_list[loop];
		memset(data_queue, 0, sizeof(websocket_data_queue_t));
		
		//data_queue->queue_mutex = PTHREAD_MUTEX_INITIALIZER;
		pthread_mutex_init(&data_queue->queue_mutex, NULL);
		
		data_queue->websocket_session_id = loop;
		data_queue->finish = 1;
	}
	ws_data_queue_init = 1;
}
