#include "mesh_rtc_inc.h"
#include "mesh_rtc_call.h"

////////////////////////////////////////////////////////////////////////
static int audio_output_flag = 1;

static const char *audio_all_output_format = "on=%d time=%d";
////////////////////////////////////////////////////////////////////////
void   audio_output_enable()
{
	audio_output_flag = 1;
}
////////////////////////////////////////////////////////////////////////
void   audio_output_disable()
{
	audio_output_flag = 0;
}
////////////////////////////////////////////////////////////////////////
int   audio_output_allowed()
{
	return audio_output_flag;
}
////////////////////////////////////////////////////////////////////////
void  ptt_capability_set(int send_allow, int recv_allow)
{
	FILE *fp;
	char buf[32];
	int cap, ret;

	cap = 0;
	if(send_allow)
	{
		cap = AV_PTT_CAP_SEND;
	}
	if(recv_allow)
	{
		cap = cap | AV_PTT_CAP_RECV;
	}
	
	//old cap info
	ret = ptt_capability_get();
	if(ret == cap)
	{
		return ; // not changed
	}
	
	fp = fopen(AV_PTT_CAP_FILE, "w+");
	if(!fp)
	{
		return ;
	}

	memset(buf, 0, 32);
	sprintf(buf, "%d", cap);
	ret = fwrite(buf, 1, strlen(buf), fp);
	fclose(fp);
	if(ret <= 0)
	{
		printf("write to file %s failed \n", AV_PTT_CAP_FILE);
	}
	
	return;
}
////////////////////////////////////////////////////////////////////////
int  ptt_capability_get()
{
	FILE *fp;
	char buf[32];
	int cap, ret;
	
	fp = fopen(AV_PTT_CAP_FILE, "r");
	if(!fp)
	{
		return -1;
	}
	
	memset(buf, 0, 32);
	ret = fread(buf, 1, 32, fp);
	fclose(fp);
	if(ret <= 0)
	{
		return -1;
	}

	ret = 0;
	cap = atoi(buf);	
	if(cap & AV_PTT_CAP_SEND)
	{
		ret = AV_PTT_CAP_SEND;
	}
	if(cap & AV_PTT_CAP_RECV)
	{
		ret = ret | AV_PTT_CAP_RECV;
	}
		
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////
int  audio_chann_all_output_get(int *all_on, u_int64_t *keep_time)
{
	char buf[32];
	int  ret, time;

	*keep_time = 0;	
	ret = tmp_status_get(AV_AUDIO_ALL_OUTPUT_FILE, buf, 32);
	if(ret <= 0)
	{
		return -1;
	}
	
	ret = sscanf(buf, audio_all_output_format, all_on, &time);
	if(ret != 2)
	{
		return -1;
	}
	if(time > 180 * 1000)  // max 180 seconds output to all audio channel
	{
		return -1;
	}
	*keep_time = time;
	
	return 1;
}
////////////////////////////////////////////////////////////////////////////////////////
int  audio_chann_all_output_set(int all_on, int keep_time)
{
	char buf[32];
	int  ret;
	
	sprintf(buf, audio_all_output_format, all_on, keep_time);
	
	ret = tmp_status_set(AV_AUDIO_ALL_OUTPUT_FILE, buf);
	if(ret <= 0)
	{
		return -1;
	}
	
	return 1;
}