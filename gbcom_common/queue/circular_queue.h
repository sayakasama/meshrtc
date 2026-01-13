#ifndef  __CIRCULAR_QUEUE_H__
#define  __CIRCULAR_QUEUE_H__


#ifdef  __cplusplus
extern "C" {
#endif

// this queue is used for fixed size unit
typedef struct tag_queue_data{
	int  size;
	char *buf;
}queue_data_t;

#define QUEUE_NAME_LEN  64
typedef struct tag_circular_queue{
    queue_data_t *data_list;
    int  head;
    int  tail;
    int  data_num;
	int  put_total_num;
	int  put_overflow_num;
	int  mem_overflow_num;
	char queue_name[QUEUE_NAME_LEN];
} circular_queue_t;

#define c_queue_t circular_queue_t

c_queue_t * circular_queue_create(int  unit_num, const char *queue_name);
int circular_queue_delete(c_queue_t *  c_queue);

int circular_queue_put(c_queue_t *queue, const char *data, int size);
int circular_queue_get(c_queue_t *queue, char *data, int *size, int max_size);


#ifdef  __cplusplus
}
#endif


#endif