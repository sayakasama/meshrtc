#include <stdio.h>
#include <string.h>
#include <errno.h>
#include  "bmp.h"
#include  "utils.h"
////////////////////////////////////////////////////////////////////////////////////		
int get_from_bmp_file(const char *file_name, unsigned char *data, int max_size, unsigned int *width, unsigned int *height, unsigned int *bpp, unsigned int *offset)
{
	bitmap_file_head *bmpheader;
	bitmap_info_head *bmpinfo;
	FILE *fp;
	static int  load_error = 0;
	unsigned int total_size, image_data_size;

	*offset = 0;
	*width = 0;
	*height = 0;
	if ((fp = fopen(file_name, "rb")) == NULL)
	{
		printf("[get_from_bmp_file]open file failed! %s error count=%d %s\n", file_name, ++load_error, strerror(errno));
		return -1;
	}

	total_size = fread(data, 1, max_size, fp);
	fclose(fp);
	if (total_size <= (sizeof(bitmap_file_head) + sizeof(bitmap_info_head)))
	{
		printf("[get_from_bmp_file] file to small!  %s \n", file_name);
		return -1;
	}

	bmpheader = (bitmap_file_head *)data;
	bmpinfo = (bitmap_info_head *)(data + sizeof(bitmap_file_head));
	if (bmpheader->bfType != BITMAP_FILE_MAGIC)
	{
		printf("[get_from_bmp_file] file bitmap Type %d != BITMAP_FILE_MAGIC %d \n", bmpheader->bfType, BITMAP_FILE_MAGIC);
		return -1;
	}

	//bmpheader.bfReserved1 = 0;
	//bmpheader.bfReserved2 = 0;
	if (bmpheader->bfOffBits != (sizeof(bitmap_file_head) + sizeof(bitmap_info_head)))
	{
		printf(" file offset %d != %d \n", bmpheader->bfOffBits, (int)((sizeof(bitmap_file_head) + sizeof(bitmap_info_head))));
		return -1;
	}

	*offset = sizeof(bitmap_file_head) + sizeof(bitmap_info_head);
	//bmpheader.bfSize      // bmpheader.bfOffBits + width * height * bpp / 8;		
	*width = bmpinfo->biWidth;
	*height = bmpinfo->biHeight;
	*bpp = bmpinfo->biBitCount;

	image_data_size = bmpinfo->biWidth * bmpinfo->biHeight * bmpinfo->biBitCount / 8;
	if ((total_size - *offset) != image_data_size)
	{

	}

	return image_data_size;
}
////////////////////////////////////////////////////////////////////////////////////	
void save_to_bmp_file(const char *file_name, unsigned char *data, int width, int height, int bpp)
{
	bitmap_file_head bmpheader;
	bitmap_info_head bmpinfo;
	FILE *fp;

	if ((fp = fopen(file_name, "wb+")) == NULL) {
		printf("open file failed! %s\n", file_name);
		return;
	}

	bmpheader.bfType = BITMAP_FILE_MAGIC;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(bitmap_file_head) + sizeof(bitmap_info_head);
	bmpheader.bfSize = bmpheader.bfOffBits + width * height*bpp / 8;

	bmpinfo.biSize = sizeof(bitmap_info_head);
	bmpinfo.biWidth = width;
	bmpinfo.biHeight = height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = 0; //0, no compress;1:RLE8; 2:RLE4,3:Bitfields  
	bmpinfo.biSizeImage = (width * bpp + 31) / 32 * 4 * height;
	bmpinfo.biXPelsPerMeter = 0; //100  
	bmpinfo.biYPelsPerMeter = 0; //100 
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(bmpheader), 1, fp);
	fwrite(&bmpinfo, sizeof(bmpinfo), 1, fp);
	fwrite(data, width*height*bpp / 8, 1, fp);

	fclose(fp);

#if 0	
//#ifndef LINUX_X86 // for debug 20240806
	char cmd[256];
	static int index=0;
	sprintf(cmd, "cp %s /data/local/photo_%d.bmp", file_name, index++);
	android_system(cmd);
#endif	
}