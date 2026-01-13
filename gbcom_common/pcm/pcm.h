#ifndef __PCM_H__
#define __PCM_H__


#ifdef  __cplusplus
extern "C" {
#endif

void  pcm_close(FILE *fp);
int   pcm_file_write(FILE *fp, char *buf, int buf_size);
FILE *pcm_open_write(const char *file_name);
FILE *pcm_open_read(const char *file_name);
int   pcm_file_read(FILE *fp, int sample, int data_bits, char *buf, int buf_size, int recycle);

#ifdef  __cplusplus
}
#endif


#endif