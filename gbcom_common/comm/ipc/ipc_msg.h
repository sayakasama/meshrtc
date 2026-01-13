#ifndef __IPC_MSG_H__
#define __IPC_MSG_H__

#define IPC_QUEUE_NUM_MAX      4
#define IPC_QUEUE_MSG_NUM      20 // buffer msg num 
#define IPC_MSG_QUEUE_MIN_SIZE 4096


#ifdef  __cplusplus
extern "C" {
#endif

///////////////////////////////////////////////////////////////
int ipc_msg_init(const char *module_name, int queue_size);
int ipc_msg_recv_inner(int msgid, char *msg, int *msg_size, int max_size);
int ipc_msg_send_inner(int msgid, const char *msg, int msg_size);

int   ipc_msg_thread_start(const char *queue_name, int msg_buf_size);
void  ipc_msg_thread_stop(int queue_num);
int   ipc_msg_get(int queue_num, char *data, int *size, int max_size);
int   ipc_msg_send(const char *module_name, const char *msg, int msg_size);

int   ipc_msg_send_ext(const char *dst_module, unsigned int msg_type, unsigned char sn, const char *msg, int msg_size);
int   ipc_msg_get_ext(int queue_num, char *src_module, unsigned int *msg_type, unsigned char *msg_sn, char *data, int *size, int max_size);

#ifdef  __cplusplus
}
#endif


#endif // __IPC_MSG_H__
