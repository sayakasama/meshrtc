#pragma once
 
#include <string>
#include <thread>

#define BITMAP_FILE_MAGIC 0x4d42

#pragma pack(1)  
typedef struct tag_bitmap_file_head
{
	unsigned short bfType;
	unsigned int bfSize;
	unsigned short bfReserved1;
	unsigned short bfReserved2;
	unsigned int bfOffBits;
}bitmap_file_head;

typedef struct tag_bitmap_info_head
{
	unsigned int biSize;
	unsigned int biWidth;
	unsigned int biHeight;
	unsigned short biPlanes;
	unsigned short biBitCount;
	unsigned int biCompression;
	unsigned int biSizeImage;
	unsigned int biXPelsPerMeter;
	unsigned int biYPelsPerMeter;
	unsigned int biClrUsed;
	unsigned int biClrImportant;
}bitmap_info_head;
#pragma pack()  

void save_to_bmp_file(const char *file_name, unsigned char *data, int width, int height, int bpp);
int  get_from_bmp_file(const char *file_name, unsigned char *data, int max_size, unsigned int *width, unsigned int *height, unsigned int *bpp, unsigned int *offset);
		