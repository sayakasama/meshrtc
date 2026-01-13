#include "stdio.h"
#include "stdlib.h"
#include "string.h"
 #include <arpa/inet.h>
 
#include "resample.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
unsigned int resample_s16(const short *input, short *output, unsigned int in_samples_rate, int out_samples_rate, unsigned int in_len, int rsv) 
{
	u_int32_t ret_len;
	double   step_dist;
	double   coeff;
	int16_t  data_1, data_2;
	int16_t  tmp;
	u_int32_t i,c;
	
    if ((input == NULL) || (output == NULL))
	{
        return 0;
    }
	
	// note: noly support 8K/16K/48K samples
	ret_len   = (u_int64_t) (in_len * (double) out_samples_rate / (double) in_samples_rate);	
	if(in_samples_rate > out_samples_rate)
	{
		step_dist = ((double) in_samples_rate / (double) out_samples_rate);
		for (i = 0; i < ret_len - 1; i++) 
		{
			data_1  = 0;
#ifndef LINUX_X86			
			for (c = 0; c < step_dist; c++) 
			{
				data_1 += (input[(int)(i*step_dist) + c]);
			}
			*output = (data_1/step_dist); // scale to normal level
#else
			for (c = 0; c < step_dist; c++) 
			{
				data_1 += ntohs(input[(int)(i*step_dist) + c]);
			}
			*output = htons(data_1/step_dist); // scale to normal level
#endif
			output++;
		}	
		// last data
#ifndef LINUX_X86					
		*output = (input[(int)(i*step_dist-1)]);
#else
		*output = htons(input[(int)(i*step_dist-1)]);	
#endif	
	}
	else
	{
		step_dist = ((double) out_samples_rate / (double) in_samples_rate);
		coeff     = 1/step_dist;
		
		for (i = 0; i < ret_len/step_dist; i++) 
		{
#ifndef LINUX_X86			
			data_1 = input[i];
			if(i == (ret_len/step_dist-1))
			{
				data_2 = input[i];
			} else {
				data_2 = input[i+1]; // last data
			}

			for (c = 0; c < step_dist; c++) 
			{
				tmp     = (double)data_1  * (1 - c * coeff) + (double)data_2 * (c * coeff);
				*output = tmp;
				output++;
			}
#else
			data_1 = ntohs(input[i]);
			if(i == (ret_len/step_dist-1))
			{
				data_2 = ntohs(input[i]);
			} else {
				data_2 = ntohs(input[i+1]); // last data
			}

			for (c = 0; c < step_dist; c++) 
			{
				tmp     = (double)data_1  * (1 - c * coeff) + (double)data_2 * (c * coeff);
				*output = htons((u_int16_t)tmp);
				output++;
			}
#endif			
		}
	}
		
    return ret_len;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef PCM_SELF_TEST
#include "pcm.h"
#include <unistd.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void resample_file_test(const char *file_in, int inSampleRate, int outSampleRate)
{
	char data_in[2048];
	char data_out[2048];	
	char file_out[512];
	int  inputSize, ret, sample_byte = 2;
	FILE *fp_out, *fp_in;
	
	sprintf(file_out, "%s.%d.pcm", file_in, outSampleRate);
	
	fp_out = pcm_open_write(file_out);
	if(!fp_out)
	{
		printf("open out file failed \n");
		return ;
	}
	
	fp_in  = pcm_open_read(file_in);
	if(!fp_in)
	{
		fclose(fp_out);
		printf("open pcm data file failed \n");
		return ;
	}
	
	inputSize = 128;
	ret       = 1;
	while(ret)
	{
		ret = pcm_file_read(fp_in, inputSize, sample_byte, data_in, 2048, 0);
		if(ret)
		{
			
			ret = resample_s16((const int16_t *)data_in, (int16_t *)data_out, inSampleRate, outSampleRate, inputSize, 1);
			pcm_file_write(fp_out, data_out, ret * sample_byte);
		}
	}
	
	fclose(fp_out);
	fclose(fp_in);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void resample_func_test()
{
	int16_t data_in[2048];
	int16_t data_out_1[2048];
	int16_t data_out_2[2048];
	int inSampleRate, outSampleRate; 
	u_int64_t inputSize = 64;
	int loop;
	
	for(loop=0; loop < 256; loop++)
	{
		data_in[loop] = htons(loop);
	}
	
	inSampleRate  = 8000;
	outSampleRate = 16000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);
	for(loop=0; loop < inputSize*2; loop++)
	{
		/*
		if(data_out_1[loop] != data_out_2[loop])
		{
			printf("[1]not equal \n");
		}*/			
		printf("%d ", data_out_1[loop]);
		if((loop+1)% 16 == 0)
		{
			printf("\n");
		}
	}
	return; // test 1
		
	inSampleRate  = 8000;
	outSampleRate = 48000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);	
	
	inSampleRate  = 16000;
	outSampleRate = 48000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);		
	
	inSampleRate  = 16000;
	outSampleRate = 8000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);			
	
	inSampleRate  = 48000;
	outSampleRate = 8000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);			
	
	inSampleRate  = 48000;
	outSampleRate = 16000; 
	inputSize     = 128;
	resample_s16((const int16_t *)data_in, (int16_t *)data_out_2, inSampleRate, outSampleRate, inputSize, 1);			
	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////	
int main(int argc, char *argv[])
{	
	int opt;
	char file[512];
	int inSampleRate; 
	int outSampleRate;
	
	resample_func_test();
	
	while ((opt = getopt(argc, argv, "i:a:b:h")) != -1)
	{
		switch (opt) {
		case 'i':
			strcpy(file, optarg); 
			printf("file=%s \n", file);
		break;

		case 'a':
			inSampleRate = atoi(optarg);
			printf("inSampleRate=%d \n", inSampleRate);			
		break;

		case 'b':
			outSampleRate = atoi(optarg);
			printf("outSampleRate=%d \n", outSampleRate);			
		break;
		
		case 'h':
		default:
			printf("specify params. for example: ./test  123.pcm 8000 16000 \n");
			return 0;
		break;
		}
	}
	
	resample_file_test(file, inSampleRate, outSampleRate);

	return 0;
}
#endif