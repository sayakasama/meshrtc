#ifndef __MESH_RTC_WEBSOCKET_H__
#define __MESH_RTC_WEBSOCKET_H__


#ifdef  __cplusplus
extern "C" {
#endif


#define  WS_MSG_BUF_NUM     4
#define  WS_SESSION_MAX_NUM 2

#define  WS_MSG_MAX_SIZE    (16*1024)
//////////////////////////////////////////////////////////////////////////
void* websocket_conn_init(const char *url, int conn_timeout);
void  websocket_close(void *p);

int   websocket_recv_text(void *p, char *out_buf, int max_size, int *type);
int   websocket_send_text(void *p, const char *data);

int   websocket_ping(void *p, const char *data);
//////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////
int  websocket_session_thread_start(const char *url);
int  websocket_session_thread_stop(int websocket_session_id);

int  websocket_session_data_dequeue(int  websocket_session_id, char *data, int *size, int max_size);
int  websocket_session_data_enqueue(int  websocket_session_id, const char *data, int size);


#ifdef  __cplusplus
}
#endif


#endif
