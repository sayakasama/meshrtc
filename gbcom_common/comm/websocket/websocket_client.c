#include <curl/curl.h>
#include <curl/websockets.h>

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "websocket_client.h"

/// CURLWS_TEXT CURLWS_CLOSE CURLWS_PING CURLWS_PONG
///////////////////////////////////////////////////////////////////////////////////////
static int websocket_send_inner(CURL *curl, const char *data, int data_len, int type)
{
	size_t sent;
	CURLcode ret;

	if (!curl)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "[websocket_send_inner]NULL POINT to curl!\n");
		return MESH_RTC_FAIL;
	}

	ret = curl_ws_send(curl, data, data_len, &sent, 0, type);
	if(CURLE_OK != ret)
	{
		return MESH_RTC_FAIL;
	}
	
	return MESH_RTC_OK;
}

///////////////////////////////////////////////////////////////////////////////////////
int websocket_ping(void *p, const char *data)
{
	CURL *curl = (CURL *)p;
	return websocket_send_inner(curl, data, strlen(data), CURLWS_PING);
}
///////////////////////////////////////////////////////////////////////////////////////
int websocket_send_text(void *p, const char *data)
{
	CURL *curl = (CURL *)p;
	return websocket_send_inner(curl, data, strlen(data), CURLWS_TEXT);
}
///////////////////////////////////////////////////////////////////////////////////////
int websocket_recv_text(void *p, char *out_buf, int max_size, int *type)
{
	size_t rlen;
	//int type;
	const struct curl_ws_frame *meta;
	CURL *curl = (CURL *)p;
	if(!curl)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "[websocket_recv_text]NULL POINT to curl!\n");
		return MESH_RTC_FAIL;
	}
	CURLcode result = curl_ws_recv(curl, out_buf, max_size, &rlen, &meta);
	if (result)
	{
		return 0; // no data
	}

	if (meta->flags & CURLWS_PONG)
	{
		if(rlen > max_size)
		{
			rlen = max_size;
		}
		memcpy(out_buf, p, rlen);

		return rlen; // no APP data
	}
	else if (meta->flags & CURLWS_TEXT)
	{
		// put msg to out queue
		return rlen; // return  APP data
	}
	else if (meta->flags & CURLWS_CLOSE)
	{
		// close all
		return MESH_RTC_FAIL; // no APP data
	}
	else 
	{
		fprintf(stderr, "websocket_recv: got %u bytes rflags %x\n", (int)rlen, meta->flags);
	}


	fprintf(stderr, "ws: curl_ws_recv returned %u, received %u\n", (unsigned int)result, (unsigned int)rlen);
	return (int)0;
}

///////////////////////////////////////////////////////////////////////////////////////
void websocket_close(void *p)
{
	size_t sent;
	CURL *curl = (CURL *)p;
	if(!curl)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "[websocket_close]NULL POINT to curl!\n");
		return;
	}
	(void)curl_ws_send(curl, "", 0, &sent, 0, CURLWS_CLOSE);

	/* always cleanup */
	curl_easy_cleanup(curl);
}

////////////////////////////////////////////////////////////
void* websocket_conn_init(const char *url, int conn_timeout)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if (!curl)
	{
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url); //"wss://example.com"
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, conn_timeout);
	curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L); /* websocket style */

	//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1); // for debug

	// wss
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0); 
	
	/* Perform the request, res will get the return code */
	res = curl_easy_perform(curl);
	/* Check for errors */
	if (res != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_perform() failed: %s error: %s\n", url, curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		//curl_global_cleanup();
		return NULL;
	}

	return curl;
}
