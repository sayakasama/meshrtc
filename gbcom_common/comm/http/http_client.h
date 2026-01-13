#ifndef  __MESH_RTC_HTTP_CLIENT_H__
#define  __MESH_RTC_HTTP_CLIENT_H__


#ifdef  __cplusplus
extern "C" {
#endif


#define HTTP_RSP_MAX_LEN     (1024*32)

#define HTTP_CLIENT_MAX_NUM  4

#define HTTP_MSG_MAX_NUM     6
/////////////////////////////////////////////////////////////
int http_client_get(const char *url, int conn_timeout, char *rsp, int *rsp_len, int max_size) ;
int http_client_post(const char *url, char *post_data, int conn_timeout, char *rsp, int *rsp_len, int max_size);

/////////////////////////////////////////////////////////
int  http_thread_start(void);
int  http_thread_stop(int http_id);

int  http_msg_dequeue(int  http_id, char *data, int *size, int max_size);
int  http_msg_enqueue(int  http_id, const char *data);


#ifdef  __cplusplus
}
#endif


#endif