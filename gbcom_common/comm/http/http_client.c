#include <curl/curl.h>
#include <curl/websockets.h>

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "http_client.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct tag_http_rsp {
	unsigned int len;
	unsigned int offset;
	unsigned int max_size;
	char        *rsp_msg;	
}http_rsp_t;

http_rsp_t  http_rsp;
extern pthread_mutex_t    http_rsp_mutex;
////////////////////////////////////////////////////////////////////////////////////////////////////////////
size_t http_rsp_get(char* buffer, size_t size, size_t count, void* data) 
{
	size_t recv_size = size * count;
	http_rsp_t  *rsp = (http_rsp_t *)data;
	
	pthread_mutex_lock(&http_rsp_mutex);				
	
	if((rsp->offset + recv_size) > rsp->max_size)
	{
		recv_size = rsp->max_size - rsp->offset;
	}
	rsp->len = rsp->offset + recv_size;
	
	//printf("recv_size=%ld %s \n", recv_size, buffer);
	memcpy(rsp->rsp_msg + rsp->offset, buffer, recv_size);
	//printf("\n+++++++++++++++++++++ rsp->rsp_msg=%s \n", rsp->rsp_msg);
	rsp->offset = rsp->len; // next pket offset

	pthread_mutex_unlock(&http_rsp_mutex);				
	
	return recv_size; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
int http_get(const char * url, const char * post_data, int conn_timeout, char *rsp, int *rsp_len) 
{
	CURL *handle;
	struct curl_slist *pList = NULL;
	int ret;
	
	handle = curl_easy_init();
	if(!handle)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[http_get] curl_easy_init failed: %s \n", url);
		return MESH_RTC_FAIL;
	}

	curl_easy_setopt(handle, CURLOPT_URL, url);
    
	curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, conn_timeout);
	
	// https 
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0); 

	// if post data exist, setup post data field
	if(post_data)
	{
		//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[http_get] %s post_data: %s \n", url, post_data);
		
		curl_easy_setopt(handle, CURLOPT_POST, 1);
		curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, strlen(post_data));
		curl_easy_setopt(handle, CURLOPT_POSTFIELDS,    post_data);

        pList = curl_slist_append(pList, "Content-Type: application/json;charset=utf-8");
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, pList);
	} else {
		//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "[http_get] %s \n ", url);
	}
	
	// not verify security if we use https
	if(strstr(url, "https"))
	{
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, false);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, false);
	}
	
	// rsp handle	
	curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, &http_rsp_get);
	curl_easy_setopt(handle, CURLOPT_WRITEDATA, &http_rsp);

	http_rsp.len    = 0;
	http_rsp.offset = 0;	
	ret = curl_easy_perform(handle);
	if(CURLE_OK == ret)
	{	
		*rsp_len = http_rsp.len;
	}
	//if(http_rsp.len)
	//printf("\n++++++++ http_rsp.len=%d ret=%d CURLE_OK=%d\n", http_rsp.len, ret, CURLE_OK);//http_rsp.rsp_msg);
	
	curl_easy_cleanup(handle);

	return MESH_RTC_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////
int http_client_get(const char *url, int conn_timeout, char *rsp, int *rsp_len, int max_size) 
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	//"www.baidu.com"	
	http_rsp.len      = 0;	
	http_rsp.rsp_msg  = rsp;
	http_rsp.max_size = max_size;
	http_get(url, NULL, conn_timeout, rsp, rsp_len);  

	//curl_global_cleanup();
	
	return MESH_RTC_OK;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////
int http_client_post(const char *url, char *post_data, int conn_timeout, char *rsp, int *rsp_len, int max_size) 
{
	curl_global_init(CURL_GLOBAL_DEFAULT);

	http_rsp.len      = 0;	
	http_rsp.rsp_msg  = rsp;
	http_rsp.max_size = max_size;
	http_get(url, post_data, conn_timeout, rsp, rsp_len);  

	//curl_global_cleanup();
	
	return MESH_RTC_OK;
}
