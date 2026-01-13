#ifndef __H264_DEV_H__
#define __H264_DEV_H__

#define MAX_H264_FRAME_SIZE (1024*1024*2) // max h264 frame is 2MB

#define PPS_FRAG  0X67
#define SPS_FRAG  0X68
///////////////////////////////////////////////////////////////////////
void  h264_close(FILE *fp);
int   h264_file_write(FILE *fp, char *buf, int buf_size);
FILE *h264_open_write(const char *file_name);
FILE *h264_open_read(const char *file_name);
int   h264_file_read(FILE *fp, char *buf, int *buf_size);
int   h264_file_read_frame(FILE *fp, char *buf, int *buf_size);

#endif