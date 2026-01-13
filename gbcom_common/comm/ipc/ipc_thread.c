#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "ipc_msg.h"
#include "circular_queue.h"

///////////////////////////////////////////////////////////////////
typedef struct tag_ipc_msg_queue {
	int   running;
	
	int   msgid;
	char  queue_name[MODULE_NAME_MAX_LEN];
	circular_queue_t *queue_out;
}ipc_msg_queue_t;	

ipc_msg_queue_t ipc_msg_queue_list[IPC_QUEUE_NUM_MAX];

static int ipc_msg_queue_init = 0;
static pthread_mutex_t  ipc_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
///////////////////////////////////////////////////////////////////
static void *thread_ipc_msg(void *argv)
{	
	ipc_msg_queue_t *msg_queue = (ipc_msg_queue_t *)argv;
	char msg_buf[IPC_MSG_MAX_SIZE];
	int  msg_size;
	int  ret;
	
	while(msg_queue->running)
	{
		sleep_ms(5);
		ret = ipc_msg_recv_inner(msg_queue->msgid, msg_buf, &msg_size, IPC_MSG_MAX_SIZE);
		if((ret != MESH_RTC_OK) || (0 == msg_size) )
		{
			continue;
		}
		
		ret = circular_queue_put(msg_queue->queue_out, msg_buf, msg_size);
		if(ret != MESH_RTC_OK)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "(%s) recv ipc msg and put to circular queue (queue len=%d) failed: %s ", msg_queue->queue_name, IPC_QUEUE_MSG_NUM, msg_buf);
		} else {
			//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "(%s) recv ipc msg and put to circular queue ok: %s ", msg_queue->queue_name, msg_buf);
		}
	}
	
	circular_queue_delete(msg_queue->queue_out);
	
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "thread_ipc_msg %s exit ", msg_queue->queue_name);
	
	return NULL;
}	
///////////////////////////////////////////////////////////////////
int ipc_msg_thread_start(const char *queue_name, int msg_buf_size)
{
	int ret, index;
	pthread_t tidSender;
	ipc_msg_queue_t *msg_queue;	

	// init queue 
	if(ipc_msg_queue_init == 0)
	{
		memset(&ipc_msg_queue_list, 0, sizeof(ipc_msg_queue_t) * IPC_QUEUE_NUM_MAX);
		pthread_mutex_init(&ipc_queue_mutex, NULL);
		ipc_msg_queue_init = 1;
	}
	
	pthread_mutex_lock(&ipc_queue_mutex);
	// get a idle queue	
	for(index = 0; index < IPC_QUEUE_NUM_MAX; index++)
	{
		msg_queue = &ipc_msg_queue_list[index];	
		if(msg_queue->running == 0)
		{
			break;
		}
	}
	
	if(index == IPC_QUEUE_NUM_MAX)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "no idle ipc msg queue available. IPC_QUEUE_NUM_MAX=%d ", IPC_QUEUE_NUM_MAX);
		for(index = 0; index < IPC_QUEUE_NUM_MAX; index++)
		{
			msg_queue = &ipc_msg_queue_list[index];	
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "[%d]current ipc msg queue name %s ", index, msg_queue->queue_name);
		}
		pthread_mutex_unlock(&ipc_queue_mutex);
		return -1;		
	}
	
	strncpy(msg_queue->queue_name, queue_name, MODULE_NAME_MAX_LEN - 1);
	
	msg_queue->msgid = ipc_msg_init(queue_name, msg_buf_size);
	if(msg_queue->msgid < 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create ipc msg queue %s failed ", msg_queue->queue_name);
		pthread_mutex_unlock(&ipc_queue_mutex);
		return -1;
	}
	
	msg_queue->queue_out = circular_queue_create(IPC_QUEUE_MSG_NUM, "ipc_msg_out");
	if(!msg_queue->queue_out)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create circular queue %s failed ", msg_queue->queue_name);
		pthread_mutex_unlock(&ipc_queue_mutex);
		return -1;		
	}
	
	msg_queue->running = 1;
	ret = pthread_create(&tidSender, NULL, thread_ipc_msg, msg_queue);
	if(ret)
	{
		circular_queue_delete(msg_queue->queue_out);
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create thread_ipc_msg %s thread failed(%d), error:%s(%d).\n",  msg_queue->queue_name, ret, strerror(errno), errno);
		pthread_mutex_unlock(&ipc_queue_mutex);
		return -1;
	}
	
	pthread_mutex_unlock(&ipc_queue_mutex);
	
	pthread_detach(tidSender);	
	
	return index;
}
///////////////////////////////////////////////////////////////////
void ipc_msg_thread_stop(int queue_num)
{
	ipc_msg_queue_t *msg_queue;	

	if(ipc_msg_queue_init == 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "attempt release an invalid ipc queue before it init queue_num=%d .\n",  queue_num);
		return;
	}
	
	if(queue_num >= IPC_QUEUE_NUM_MAX)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "attempt to release an invalid ipc queue  queue_num=%d .\n",  queue_num);
		return;
	}
	
	pthread_mutex_lock(&ipc_queue_mutex);

	msg_queue = &ipc_msg_queue_list[queue_num];	
	if(msg_queue->running == 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "attempt to release an idle ipc queue  queue_num=%d .\n",  queue_num);
		pthread_mutex_unlock(&ipc_queue_mutex);
		return;
	}
	
	msg_queue->running = 0;
	MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "release ipc queue %s queue_num=%d .\n",  msg_queue->queue_name, queue_num);

	pthread_mutex_unlock(&ipc_queue_mutex);
}
///////////////////////////////////////////////////////////////////
int ipc_msg_get(int queue_num, char *data, int *size, int max_size)
{
	ipc_msg_queue_t *msg_queue;	
	int ret;

	*size = 0;
	if(ipc_msg_queue_init == 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "attempt get msg from an ipc queue before it init queue_num=%d .\n",  queue_num);
		return 0;
	}

	if(queue_num >= IPC_QUEUE_NUM_MAX)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc_msg_get: an invalid ipc queue  queue_num=%d .\n",  queue_num);
		return 0;
	}
	
	pthread_mutex_lock(&ipc_queue_mutex);
	
	msg_queue = &ipc_msg_queue_list[queue_num];
	if(msg_queue->running == 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc_msg_get: an idle ipc queue queue_num=%d .\n", queue_num);
		pthread_mutex_unlock(&ipc_queue_mutex);
		return 0;
	}
	
	ret = circular_queue_get(msg_queue->queue_out, data, size, max_size);

	pthread_mutex_unlock(&ipc_queue_mutex);

	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////
int   ipc_msg_get_ext(int queue_num, char *src_module, unsigned int *msg_type, unsigned char *msg_sn, char *msg_data, int *size, int max_size)
{
	int ret, len;
	char  data[IPC_MSG_MAX_SIZE];
	common_msg_head_t *msg_head = (common_msg_head_t *)data;
	
	*size = 0;
	ret = ipc_msg_get(queue_num, data, &len, IPC_MSG_MAX_SIZE);
	if(ret <= 0)
	{
		return ret;
	}
	
	*msg_type = msg_head->msg_type;
	*msg_sn   = msg_head->msg_sn;
	module_name_get(msg_head->src_module_id, src_module);
	if(max_size < msg_head->msg_len)
	{
		return 0;
	}
	
	*size = msg_head->msg_len;
	memcpy(msg_data, msg_head->msg_data, msg_head->msg_len);
	
	return msg_head->msg_len;
}
