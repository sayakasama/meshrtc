#include "mesh_rtc_inc.h"

#include "circular_queue.h"

// Do not use queue in multi thread 
//////////////////////////////////////////////////////////////////////
c_queue_t *  circular_queue_create(int  unit_num, const char *queue_name)
{
	circular_queue_t *c_queue;
	
	if(0 == unit_num)
	{
		return NULL;
	}
	
	c_queue = (circular_queue_t *)malloc(sizeof(circular_queue_t));
	if(!c_queue)
	{
		return NULL;
	}

	strncpy(c_queue->queue_name, queue_name, QUEUE_NAME_LEN);
	
	c_queue->data_list = (queue_data_t *)malloc(sizeof(queue_data_t) * unit_num);
	if(!c_queue->data_list)
	{
		free(c_queue);
		MESH_LOG_PRINT(COMMON_MODULE_NAME,LOG_ERR,"[%s] failed: circular_queue_create malloc data list size=%d \n", c_queue->queue_name, (int)sizeof(queue_data_t) * unit_num);				
		return NULL;
	}
	
	c_queue->put_total_num     = 0;	
	c_queue->mem_overflow_num  = 0;
	c_queue->put_overflow_num  = 0;
	c_queue->head = c_queue->tail = 0;
	c_queue->data_num = unit_num;
	
	return c_queue;
}
//////////////////////////////////////////////////////////////////////
int circular_queue_put(c_queue_t *c_queue, const char *data, int size)
{
	queue_data_t *queue_data;
	
	if(!c_queue)
	{
		return MESH_RTC_FAIL;
	}

	c_queue->put_total_num++;
	if((c_queue->head + 1)%c_queue->data_num == c_queue->tail)
	{
		c_queue->put_overflow_num++;
		if(c_queue->put_overflow_num % 100 == 0)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "[%s] failed: circular_queue_put queue is full. queue len=%d discard=%d put_total_num=%d\n", c_queue->queue_name, c_queue->data_num, c_queue->put_overflow_num, c_queue->put_total_num);
		}
		return MESH_RTC_FAIL;
	}
	
	queue_data = &c_queue->data_list[c_queue->head]; 
	
	queue_data->buf = malloc(size);
	if(!queue_data->buf)
	{
		c_queue->mem_overflow_num++;
		if(c_queue->mem_overflow_num % 100 == 0)
		{		
			MESH_LOG_PRINT(COMMON_MODULE_NAME,LOG_ERR,"[%s] failed: circular_queue_put alloc buf failed %d. mem overflow num=%d\n", c_queue->queue_name, size, c_queue->mem_overflow_num);
		}
		return MESH_RTC_FAIL;		
	}
	
	memcpy(queue_data->buf, data, size);
	queue_data->size = size;
	c_queue->head = (c_queue->head + 1) % c_queue->data_num;
	
	return MESH_RTC_OK;
}
//////////////////////////////////////////////////////////////////////
int circular_queue_get(c_queue_t *c_queue, char *data, int *size, int max_size)
{
	queue_data_t *queue_data;
	int ret;
	
	*size = 0;
	if(!c_queue)
	{
		return MESH_RTC_FAIL;
	}

	if(c_queue->head == c_queue->tail)
	{
		//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO," [%s] failed: circular_queue_get queue is void\n", c_queue->queue_name);				
		return 0;
	}
	
	queue_data = &c_queue->data_list[c_queue->tail]; 
	
	if(queue_data->size <= max_size)
	{
		memcpy(data, queue_data->buf, queue_data->size);
		*size = queue_data->size;
		ret   = MESH_RTC_OK;
	} else {	
		*size = 0;
		ret   = 0;
		MESH_LOG_PRINT(COMMON_MODULE_NAME,LOG_INFO,"[%s] failed: circular_queue_get data too big %d >  buf size %d\n", c_queue->queue_name, queue_data->size , max_size);				
	}
	
	// release queue_data 
	free(queue_data->buf);
	queue_data->buf = NULL;
	c_queue->tail = (c_queue->tail + 1) % c_queue->data_num;
	return ret;
}
//////////////////////////////////////////////////////////////////////
int circular_queue_delete(c_queue_t *  c_queue)
{
	queue_data_t *queue_data;
	int total_size = 0;
	
	// release all data buf. 
	while(c_queue->head != c_queue->tail)
	{
		queue_data = &c_queue->data_list[c_queue->tail]; 	
		if(queue_data->buf) 
		{
			total_size += queue_data->size;
			free(queue_data->buf);
			queue_data->buf = NULL;
		}
		c_queue->tail = (c_queue->tail + 1) % c_queue->data_num;
	}
	if(total_size)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "data queue %s free memory : %d ", c_queue->queue_name, total_size);
	}
	
	// release queue buf
	free(c_queue);
	
	return MESH_RTC_OK;
}
