#include <stdio.h>
#include <stdlib.h>
#include "soapH.h"
#include "Hello_USCOREBinding.nsmap"

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

/////////////////////////////////////////////////////////////////////////////////////

int my_system(const char * cmdstring);
/////////////////////////////////////////////////////////////////////////////////////
char ret_content[GSOAP_MAX_RET_SIZE];

char *ret_file = "/tmp/rpc_ret.txt";
char *log_file = "./av_module.log";
FILE *log_fd   = NULL;
int  file_flush_size = 32; // default is 4096; 32 for debug

char *start_line = "////////////////////// start record log info from av modules //////////////////////////////// \n";
/////////////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
	int m, s; /* master and slave sockets */
	int port = GSOAP_DEFAULT_PORT;
	struct soap hello_soap;
	
	soap_init(&hello_soap);
	hello_soap.send_timeout     = 5;
	hello_soap.recv_timeout     = 5;   /* 5 sec socket idle timeout */
    hello_soap.transfer_timeout = 30;  /* 30 sec message transfer timeout */
  
	if (argc >= 2)
	{
		port = atoi(argv[1]);
	}
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "server_port=%d \n", port);

	{
		m = soap_bind(&hello_soap, NULL, port, 100);
		if (m < 0)
		{
			soap_print_fault(&hello_soap, stderr);
			exit(-1);
		}
		
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "Socket connection successful: master socket = %d\n", m);
		for (; ; )
		{
			s = soap_accept(&hello_soap);
			if (!soap_valid_socket(s))
			{
				soap_print_fault(&hello_soap, stderr);
				exit(-1);
			}
			
			MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "Socket connection successful: slave socket = %d\n", s);
			soap_serve(&hello_soap);// start service && wait for request
			soap_end(&hello_soap);
		}
	}
	
	soap_print_fault(&hello_soap, stderr);
	soap_destroy(&hello_soap); /* delete deserialized objects */
	soap_end(&hello_soap);     /* delete heap and temp data */
	soap_free(&hello_soap);    /* we're done with the context */
  
	return 0;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__sayHello(struct soap *hello_soap, 
    char*      req,	///< Input parameter, :unqualified name as per RPC encoding
    char*      *rsp	///< Output parameter, :unqualified name as per RPC encoding
)
{
	char cmd_buf[1024];
	FILE *fd;
	
	if(0 == strcmp(req, "hello"))
	{
		*rsp = soap_strdup(hello_soap, "ok");
	}
	else if(0 == strcmp(req, "reboot"))
	{
		// write a tmp shell file
		sprintf(cmd_buf, "echo \"sleep 5 && reboot \" > %s ", ret_file);
		my_system(cmd_buf);
		// set file excuteable
		sprintf(cmd_buf, "chmod +x %s ", ret_file);
		my_system(cmd_buf);
		
		*rsp = soap_strdup(hello_soap, "reboot after 5 seconds");
		
		// excute shell file to reboot lately
		sprintf(cmd_buf, "sh %s & ", ret_file);
		my_system(cmd_buf);		
	}
	else
	{
		// dump to file for debug
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "execute: %s > %s ", req, ret_file);
		sprintf(cmd_buf, "%s > %s ", req, ret_file);
		my_system(cmd_buf);
		fd = fopen(ret_file, "r");
		if(fd)
		{
			memset(ret_content, 0, GSOAP_MAX_RET_SIZE);
			fread(ret_content, 1, GSOAP_MAX_RET_SIZE, fd);
			*rsp = soap_strdup(hello_soap, ret_content);
			fclose(fd);	
		}
	}
	
	if(NULL == *rsp)
	{
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "[ns1__sayHello]req=%s rsp addr is null \n", req);		
        char *s = (char*)soap_malloc(hello_soap, 1024);
        sprintf(s, "rsp addr is null?");
        soap_sender_fault(hello_soap, "rsp pointer is null", s);		
		return -1;
	}
	
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "req is: %s rsp is: %s \n", req, *rsp);
	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__audio_sample_set(struct soap *hello_soap, int sample, int *ret)
{
	*ret    = MESH_RTC_OK;
	// TODO: set audio dev 
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "set audio sample: %d \n", sample);
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__audio_sample_get(struct soap *hello_soap, int *sample)
{	
	*sample = 0;
	
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get audio sample: %d \n", *sample);	
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__audio_button_status_get(struct soap *hello_soap, int *status)
{	
	*status = 0;
	
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get audio button status: %d \n", *status);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_fps_get(struct soap *hello_soap, int *fps)
{	
	*fps = 0;
	
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get video fps: %d \n", *fps);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_fps_set(struct soap *hello_soap, int fps, int *ret)
{	
	*ret = MESH_RTC_OK;
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "set video fps: %d \n", fps);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_wh_get(struct soap *hello_soap, int *w, int *h)
{	
	*w = 0;
	*h = 0;
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get video w=%d h=%d \n", *w, *h);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_wh_set(struct soap *hello_soap, int w, int h, int *ret)
{	
	*ret = MESH_RTC_OK;
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "set video w=%d h=%d ret=%d \n", w, h, *ret);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_bitrate_get(struct soap *hello_soap, int *bitrate_max, int *bitrate_min)
{	
	*bitrate_max = 0; 
	*bitrate_min = 0;
	
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get video bitrate_max=%d bitrate_min=%d \n", *bitrate_max, *bitrate_min);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_bitrate_set(struct soap *hello_soap, int bitrate_max, int bitrate_min, int *ret)
{	
	*ret = MESH_RTC_OK;
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "set video bitrate_max=%d bitrate_min=%d ret=%d \n", bitrate_max, bitrate_min, *ret);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_interval_get(struct soap *hello_soap, int *i_interval)
{	
	*i_interval = 0;
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "get video i_interval=%d \n", *i_interval);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_interval_set(struct soap *hello_soap, int i_interval, int *ret)
{	
	*ret = MESH_RTC_OK;
	//TODO
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "set video i_interval=%d ret=%d \n", i_interval, *ret);	
	return SOAP_OK;
}

/////////////////////////////////////////////////////////////////////////////////////
int ns1__log_info(struct soap *hello_soap, const char *info)
{	
	char line_end  = '\n';
	int  info_size ;
	static int log_total_size = 0;
	static int log_flush_size = 0;
	
	if(!log_fd)
	{
		log_fd = fopen(log_file, "a+");
	}
	
	if(!log_fd)
	{
		MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, " ns__log_info file=%s : %s errno=%d \n", log_file, strerror(errno), errno);	
		return SOAP_OK;
	}
	
	// first log message
	if(0 == log_total_size)
	{
		fwrite(start_line, 1, strlen(start_line), log_fd);
		fflush(log_fd);
	}
	
	info_size = strlen(info);
	fwrite(info, 1, info_size, log_fd);
	fwrite(&line_end, 1, 1, log_fd);	
	
	log_total_size += (info_size + 1);
	log_flush_size += (info_size + 1);
	
	if(log_total_size > file_flush_size)
	{
		fflush(log_fd);
		log_flush_size = 0;
	}
	
	MESH_LOG_PRINT(RPC_SERVER_MODULE_NAME, LOG_INFO, "log from av: %s ", info);
	
	return SOAP_OK;
}
///////////////////////////////////////////////////////////////////
int my_system(const char * cmdstring)
{
    pid_t pid;
    int status;
 	 
 	if(cmdstring == NULL)
 	{
 	    return (1); 
 	}
		
 	if((pid = fork())<0)
 	{
 	    status = -1; 
 	}
 	else if(pid == 0)
 	{
		// child process
 	    execl("/system/bin/sh", "sh", "-c", cmdstring, (char *)0);
 	    _exit(127); 
 	}
 	else 
 	{
		// parent process
 	    while(waitpid(pid, &status, 0) < 0)
 	    {
 	        if(errno != EINTR)
 	        {
 	            status = -1; //
 	            break;
 	        }
 	    }
 	}
 	 
    return status; 
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_cfg_set(struct soap *hello_soap, char*  cfg_data, int len, int *ret)
{
	// do nothing
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__video_cfg_get(struct soap *hello_soap, char* *cfg_data)
{
	// do nothing
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__audio_cfg_set(struct soap *hello_soap, char*  cfg_data, int len, int *ret)
{
	// do nothing
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__audio_cfg_get(struct soap *hello_soap, char* *cfg_data)
{
	// do nothing
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__status_notify(struct soap *hello_soap, const char *info)
{	
	// DO nothing	
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__hdmi_wh_set(struct soap *hello_soap, int w, int h, int *ret)
{	
	// DO nothing	
	return SOAP_OK;
}
/////////////////////////////////////////////////////////////////////////////////////
int ns1__osd_text_set(struct soap *hello_soap, char*  text_data, int len, int *ret)
{
	// do nothing
	return SOAP_OK;
}