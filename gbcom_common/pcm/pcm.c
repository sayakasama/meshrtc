#include "mesh_rtc_inc.h" 

#include "pcm.h"
//////////////////////////////////////////////////////////////////
FILE * pcm_open_read(const char *file_name)
{
	FILE *fp;
	
	if(!file_name)
	{
		return NULL;
	}
	
	fp = fopen(file_name, "rb");
	if(!fp)
	{
		printf("pcm_open_read %s failed \n", file_name);		
		return NULL;
	}

	printf("pcm_open_read %s ok \n", file_name);				
	return fp;
}
//////////////////////////////////////////////////////////////////
int  pcm_file_read(FILE *fp, int samplePerFrame, int BytesPerSample, char *buf, int buf_size, int recycle)
{
	int size = (samplePerFrame * BytesPerSample) ;/// 8;
	int ret;
	
	if(size >  buf_size)
	{
		size = buf_size;
	}
	
	ret = fread(buf, 1, size, fp);
	if(ret <= 0)
	{
		if(recycle)
		{
			fseek(fp, 0, SEEK_SET); // re-read file
		} else {
			return 0;
		}
	}
	
	return ret;
}

//////////////////////////////////////////////////////////////////
FILE * pcm_open_write(const char *file_name)
{
	FILE *fp;
	
	if(!file_name)
	{
		return NULL;
	}
	
	fp = fopen(file_name, "wb");
	if(!fp)
	{
		printf("pcm_open_write %s failed \n", file_name);				
		return NULL;
	}
	
	printf("pcm_open_write %s ok \n", file_name);
		
	return fp;
}
//////////////////////////////////////////////////////////////////
int  pcm_file_write(FILE *fp, char *buf, int buf_size)
{
	int ret;
	
	ret = fwrite(buf, 1, buf_size, fp);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
void pcm_close(FILE *fp)
{
	fclose(fp);
}


