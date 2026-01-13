#include "mesh_rtc_inc.h"

#include "h264.h"

// used in single thread
char h264_read_buf[MAX_H264_FRAME_SIZE];
int  cur_offset = 0;
int  data_len   = 0;

char h264_split_code_3[3] = {0, 0, 1};
char h264_split_code_4[4] = {0, 0, 0, 1};
//////////////////////////////////////////////////////////////////
FILE * h264_open_read(const char *file_name)
{
	FILE *fp;
	
	if(!file_name)
	{
		return NULL;
	}
	
	fp = fopen(file_name, "rb");
	if(!fp)
	{
		return NULL;
	}
	
	return fp;
}
//////////////////////////////////////////////////////////////////
int  h264_file_read(FILE *fp, char *buf, int *buf_size)
{
	int size = 0; 
	int ret;
	
	// TODO. find a frame and copy it to buf
	
	if(size >  *buf_size)
	{
		size = *buf_size;
	}
	
	ret = fread(buf, 1, size, fp);
	*buf_size = ret;
	
	return ret;
}
//////////////////////////////////////////////////////////////////
int   h264_file_read_frame(FILE *fp, char *buf, int *buf_size)
{
	int size, ret;
	char *p, *start_p, *end_p;
	int start_split, end_split;
	
	// read new data from file
	if(data_len == 0)
	{
		size     = MAX_H264_FRAME_SIZE - cur_offset;
		ret      = fread(h264_read_buf + cur_offset, 1, size, fp);		
		if(ret <=0)
		{
			return MESH_RTC_FAIL;
		}		
		data_len = 	cur_offset + ret; // total data now
	}
	
	start_split = 0;
	// search next split_code
	p = h264_read_buf + cur_offset;
	start_p = p;
	while(cur_offset < data_len)
	{
		if(0 == memcmp(p, h264_split_code_3, 3))
		{
			start_split = 1;
			break;
		}
		if(0 == memcmp(p, h264_split_code_4, 4))
		{
			start_split = 1;
			break;
		}	
		p++;
	}
	if(0 == start_split)
	{
		if(MAX_H264_FRAME_SIZE == data_len)
		{
			return MESH_RTC_FAIL; // invalid h264? frame too big?
		} else {
			// read more data ?
		}
	}
	
	p = p + sizeof(h264_split_code_4); // skip prev split code

	// search next split code
	end_split = 0;
	while(cur_offset < data_len)
	{
		if(0 == memcmp(p, h264_split_code_3, 3))
		{
			start_split = 1;
			break;
		}
		if(0 == memcmp(p, h264_split_code_4, 4))
		{
			start_split = 1;
			break;
		}	
		p++;
	}
	
	if(0 == end_split)
	{
		if(cur_offset < MAX_H264_FRAME_SIZE/2)
		{
			// invalid h264? frame too big? no other split code from start to the end
			return MESH_RTC_FAIL; 
		} else {
			// move data to head
			memcpy(h264_read_buf, h264_read_buf + cur_offset, MAX_H264_FRAME_SIZE - cur_offset);
			cur_offset = MAX_H264_FRAME_SIZE - cur_offset;
			// read more data
			data_len = 0;
		}
	}
	end_p = p;
	
	size        = end_p - start_p;
	cur_offset += size; 	// move buf to next frame

	if(*buf_size < size)
	{
		*buf_size   = 0;
		return MESH_RTC_FAIL; // too small buf!!!
	}
	*buf_size   = size;
	memcpy(buf, start_p, size);
	
	 MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "get a h264 frame size=%d ", size);
	
	return MESH_RTC_OK;
}
//////////////////////////////////////////////////////////////////
FILE * h264_open_write(const char *file_name)
{
	FILE *fp;
	
	if(!file_name)
	{
		return NULL;
	}
	
	fp = fopen(file_name, "wb");
	if(!fp)
	{
		return NULL;
	}
	
	return fp;
}
//////////////////////////////////////////////////////////////////
int  h264_file_write(FILE *fp, char *buf, int buf_size)
{
	int ret;
	
	ret = fwrite(buf, 1, buf_size, fp);
	
	return ret;
}
//////////////////////////////////////////////////////////////////
void h264_close(FILE *fp)
{
	fclose(fp);
}


