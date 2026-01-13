#include <stdio.h>
#include <stdlib.h>
#include "soapH.h"
#include "Hello_USCOREBinding.nsmap"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"
#include "utils.h"

#include "curl_base64.h"

#include "av_api.h"
///////////////////////////////////////////////////////////////////////////////
int hello( char *first, char *rsp);
int log_info( char *info);

//static char  rsp[1024 * 16]; // max 16k rsp 
static char  api_server_addr[256]; // such as: "http://127.0.0.1:4000";	
///////////////////////////////////////////////////////////////////////////////
#ifdef RPC_CLIENT_TEST
int main(int argc, char **argv)
{
	int   result = -1;
	char  *addr;	
	char  *req;
	char  cmd[1024];

	int   sample, fps, interval, button_status;
	int   bitrate_max, bitrate_min;
	int   w,h;
	
	addr = argv[1];
	if(addr)
	{
		av_api_init(addr);
	} else {
		av_api_init("http://127.0.0.1:4567");
	}
	
	req = argv[2];
	if(req)
	{
		if(0 == strcmp(req, "log"))
		{
			result = log_info(argv[3]);
			return 0;
		} else {
			strcpy(cmd, req);
		}
	} else {
		strcpy(cmd, "hello");
	}
	MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "execute remote cmd: %s at %s \n", req, api_server_addr);
	
	result = hello(cmd, rsp);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "rsp=%s \n\n", rsp);
	}
	
	result = audio_sample_get(&sample);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "audio_sample_get sample=%d\n", sample);
	}
	
	button_status = 0;
	result = audio_button_status_get(&button_status);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "audio_button_status_get button_status=%d\n", button_status);
	}
	
	sample = 8000;
	result = audio_sample_set(sample);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "audio_sample_set sample=%d\n\n", sample);
	}

	result = video_fps_get(&fps);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_fps_get fps=%d\n", fps);
	}
	
	fps = 20;
	result = video_fps_set(fps);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_fps_set fps=%d\n", fps);
	}
	
	result = video_interval_set(interval);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_interval_set interval=%d\n", interval);
	}
	
	result = video_interval_get(&interval);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_interval_get interval=%d\n", interval);
	}	
	
	w = 640;
	h = 480;
	result = video_wh_set(w, h);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_wh_set w=%d h=%d\n", w, h);
	}
	
	result = video_wh_get(&w, &h);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_wh_get w=%d h=%d\n", w, h);
	}
	
	result = video_bitrate_set(bitrate_max, bitrate_min);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_bitrate_set bitrate_max=%d bitrate_min=%d\n", bitrate_max, bitrate_min);
	}
	
	result = video_bitrate_get(&bitrate_max, &bitrate_min);
	if (result != MESH_RTC_OK)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap err,errcode = %d\n", result);
	}
	else
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_bitrate_get bitrate_max=%d bitrate_min=%d\n\n", bitrate_max, bitrate_min);
	}
	
	return 0;
}
#endif
//////////////////////////////////////////////////////////////////
void av_api_init(const char *addr)
{
	strcpy(api_server_addr, addr);
}
//////////////////////////////////////////////////////////////////
int hello( char *cmd_req, char *cmd_rsp)
{
	struct soap add_soap;
	int result = MESH_RTC_OK;
	char *soap_rsp;
	u_int64_t time = sys_time_get();
	
	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	 
	soap_call_ns1__sayHello(&add_soap, api_server_addr, "", cmd_req, &soap_rsp);
	if (add_soap.error)
	{
		time = sys_time_get() - time;
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "spend time=%lu server_addr=%s cmd_req=%s soap error:%d,%s,%s\n", time, api_server_addr, cmd_req, add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		result = MESH_RTC_FAIL;
	} else {
		strcpy(cmd_rsp, soap_rsp);
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return result;
}
//////////////////////////////////////////////////////////////////
int log_info(char *info)
{
	struct soap add_soap;
	int result = MESH_RTC_OK;

	if(!info)
	{
		return MESH_RTC_FAIL;
	}
		
	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_send_ns1__log_info(&add_soap, api_server_addr, "", info);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		result = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return result;
}

//////////////////////////////////////////////////////////////////
int audio_sample_set( int sample)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_call_ns1__audio_sample_set(&add_soap, api_server_addr, "", sample, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int audio_sample_get( int *sample)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*sample = 0;
	soap_call_ns1__audio_sample_get(&add_soap, api_server_addr, "", sample);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int audio_button_status_get( int *button_status)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*button_status = 0;
	soap_call_ns1__audio_sample_get(&add_soap, api_server_addr, "", button_status);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int video_fps_set( int fps)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_call_ns1__video_fps_set(&add_soap, api_server_addr, "", fps, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int video_fps_get( int *fps)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*fps = 0;
	soap_call_ns1__video_fps_get(&add_soap, api_server_addr, "", fps);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int video_interval_set( int interval)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_call_ns1__video_interval_set(&add_soap, api_server_addr, "", interval, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int video_interval_get( int *interval)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*interval = 0;
	soap_call_ns1__video_interval_get(&add_soap, api_server_addr, "", interval);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int video_wh_set( int w, int h)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_call_ns1__video_wh_set(&add_soap, api_server_addr, "", w, h, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int video_wh_get( int *w, int *h)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*w = 0;
	*h = 0;
	soap_call_ns1__video_wh_get(&add_soap, api_server_addr, "", w, h);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int video_bitrate_set(int bitrate_max, int bitrate_min)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	soap_call_ns1__video_bitrate_set(&add_soap, api_server_addr, "", bitrate_max, bitrate_min, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int video_bitrate_get(int *bitrate_max, int *bitrate_min)
{
	struct soap add_soap;
	int    ret = MESH_RTC_OK;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	*bitrate_max = 0;
	*bitrate_min = 0;

	soap_call_ns1__video_bitrate_get(&add_soap, api_server_addr, "", bitrate_max, bitrate_min);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int video_cfg_set(char*  cfg_data, int len)
{
	struct soap add_soap;
	int    ret;
	size_t enc_len;
	char   *buf;
	
	soap_init(&add_soap);
	add_soap.recv_timeout     = 2;
    add_soap.send_timeout     = 2;
    add_soap.connect_timeout  = 2;
	add_soap.transfer_timeout = 3;
	
	ret = mesh_base64_encode(cfg_data, len, &buf, &enc_len);
	if(ret <= 0)						
	{
		soap_end(&add_soap);
		soap_done(&add_soap);		
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;		
		return ret;
	}
	
	ret = MESH_RTC_OK;
	soap_call_ns1__video_cfg_set(&add_soap, api_server_addr, "", buf, enc_len, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	free(buf);
	
	//MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "video_cfg_set soap enc_len=%ld\n", enc_len);
		
	return ret;
}

//////////////////////////////////////////////////////////////////
int video_cfg_get(char*  cfg_data)
{
	struct soap add_soap;
	char *soap_rsp;

	unsigned char  *buf;
	int ret;
	size_t dec_len;	

	soap_init(&add_soap);
	add_soap.recv_timeout     = 2;
    add_soap.send_timeout     = 2;
    add_soap.connect_timeout  = 2;
	add_soap.transfer_timeout = 3;
		
	soap_call_ns1__video_cfg_get(&add_soap, api_server_addr, "", &soap_rsp);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		soap_end(&add_soap);
		soap_done(&add_soap);
		return MESH_RTC_FAIL;
	} 
	
	ret = mesh_base64_decode(soap_rsp, &buf, &dec_len);
	if(ret < 0)
	{
		ret = MESH_RTC_FAIL;	
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "video params decode fail : input data len=%ld ret=%d \n", strlen(soap_rsp), ret);	
	} else {
		ret = MESH_RTC_OK;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);

	return ret;
}
//////////////////////////////////////////////////////////////////
int audio_cfg_set(char*  cfg_data, int len)
{
	struct soap add_soap;
	int    ret;
	size_t enc_len;
	char   *buf;

	soap_init(&add_soap);
	add_soap.recv_timeout     = 2;
    add_soap.send_timeout     = 2;
    add_soap.connect_timeout  = 2;
	add_soap.transfer_timeout = 3;
	
	ret = mesh_base64_encode(cfg_data, len, &buf, &enc_len);
	if(ret <= 0)						
	{
		soap_end(&add_soap);
		soap_done(&add_soap);		
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;		
		return ret;
	}

	ret = MESH_RTC_OK;
	soap_call_ns1__audio_cfg_set(&add_soap, api_server_addr, "", buf, enc_len, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	free(buf);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
int audio_cfg_get(char*  cfg_data)
{	
	struct soap add_soap;
	char *soap_rsp;

	unsigned char  *buf;
	int ret;
	size_t dec_len;	

	soap_init(&add_soap);
	add_soap.recv_timeout     = 2;
    add_soap.send_timeout     = 2;
    add_soap.connect_timeout  = 2;
	add_soap.transfer_timeout = 3;
	
	soap_call_ns1__audio_cfg_get(&add_soap, api_server_addr, "", &soap_rsp);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "audio get: soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		soap_end(&add_soap);
		soap_done(&add_soap);
		return MESH_RTC_FAIL;
	} 
	
	ret = mesh_base64_decode(soap_rsp, &buf, &dec_len);
	if(ret < 0)
	{
		ret = MESH_RTC_FAIL;	
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "audio params decode fail : input data len=%ld ret=%d \n", strlen(soap_rsp), ret);	
	} else {
		ret = MESH_RTC_OK;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);

	return ret;
}

//////////////////////////////////////////////////////////////////
int osd_text_set(char*  photo_data, int len)
{
	struct soap add_soap;
	int    ret;
	size_t enc_len;
	char   *buf;

	soap_init(&add_soap);
	add_soap.recv_timeout    = 2;
    add_soap.send_timeout    = 2;
    add_soap.connect_timeout = 2;
	
	ret = mesh_base64_encode(photo_data, len, &buf, &enc_len);
	if(ret <= 0)						
	{
		soap_end(&add_soap);
		soap_done(&add_soap);		
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;		
		return ret;
	}

	ret = MESH_RTC_OK;
	soap_call_ns1__osd_text_set(&add_soap, api_server_addr, "", buf, enc_len, &ret);
	if (add_soap.error)
	{
		MESH_LOG_PRINT(RPC_CLIENT_MODULE_NAME, LOG_INFO, "soap error:%d,%s,%s\n", add_soap.error, *soap_faultcode(&add_soap), *soap_faultstring(&add_soap));
		ret = MESH_RTC_FAIL;
	}
	
	soap_end(&add_soap);
	soap_done(&add_soap);
	
	free(buf);
	
	return ret;
}

//////////////////////////////////////////////////////////////////
