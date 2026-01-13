#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include "agc.h"
#include "audio_agc.h"

#ifndef MIN
#define  MIN(A, B)        ((A) < (B) ? (A) : (B))
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static WebRtcAgcConfig agc_config;
static void           *agc_inst = NULL;

//  kAgcModeAdaptiveAnalog  
//  kAgcModeAdaptiveDigital 
//  kAgcModeFixedDigital 
static int16_t agc_mode = kAgcModeAdaptiveDigital;
///////////////////////////////////////////////////////////////////
int audio_agc_init(int gain, int targetDb, int sample_rate)
{
	int min_level = 0;
    int max_level = 255;
	int status;
	
    agc_config.compressionGaindB = gain; // default lift 12 dB
    agc_config.limiterEnable     = 1; // default kAgcTrue (on)
    agc_config.targetLevelDbfs   = targetDb; // default 3 (-3 dBOv)	
		
    agc_inst = WebRtcAgc_Create();
    if (agc_inst == NULL) 
	{
		return -1;
	}

    status = WebRtcAgc_Init(agc_inst, min_level, max_level, agc_mode, sample_rate);
    if (status != 0) 
	{
        printf("WebRtcAgc_Init fail\n");
		audio_agc_free();
        return -1;
    }
	
    status = WebRtcAgc_set_config(agc_inst, agc_config);
    if (status != 0) 
	{
        printf("WebRtcAgc_set_config fail\n");
		audio_agc_free();
        return -1;
    }

	return 1;
}
///////////////////////////////////////////////////////////////////
void audio_agc_free()
{
	if(agc_inst)
	{
		WebRtcAgc_Free(agc_inst);
	}
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int audio_agc_proc(int16_t *data_in, uint32_t sample_rate, size_t samples_count) 
{
	int nAgcRet;
	size_t remained_samples;
	int16_t *data_out;
		
	if (agc_inst == NULL)
	{
		return -1;
	}

    if (data_in == NULL)    return -1;
    if (samples_count == 0) return -1;

    size_t samples = MIN(160, sample_rate / 100);	
	if (samples == 0)      
	{
		return -1;	
	}
    size_t total   = samples_count / samples;
	if (total == 0)      
	{
		return -1;	
	}
	
	data_out = (int16_t *)malloc(samples_count*2);    
	if (data_out == NULL)
	{
		return -1;
	}
	
    size_t num_bands = 1;
    int inMicLevel, outMicLevel = -1;
    uint8_t saturationWarning = 1;  //overflow: max value > 65536
    int16_t echo = 0;      //echo calc?

    for (int i = 0; i < total; i++) 
	{
        inMicLevel = 0;
        nAgcRet = WebRtcAgc_Process(agc_inst, (const int16_t *const *) &data_in, num_bands, samples,
                                        (int16_t *const *) &data_out, inMicLevel, &outMicLevel, echo,
                                        &saturationWarning);
        if (nAgcRet != 0) 
		{
            printf("WebRtcAgc_Process failed  %d\n", nAgcRet);
			free(data_out);
            return -1;
        }
        memcpy(data_in, data_out, samples * sizeof(int16_t));
        data_in += samples;
    }

    remained_samples = samples_count - total * samples;
    if (remained_samples <= 0) 
	{
		free(data_out);
		return 1;
	}

    // remain data process
    if (total > 0) 
	{
        data_in = data_in - samples + remained_samples;
    }
    inMicLevel = 0;
    nAgcRet = WebRtcAgc_Process(agc_inst, (const int16_t *const *) &data_in, num_bands, samples,
                                        (int16_t *const *) &data_out, inMicLevel, &outMicLevel, echo,
                                        &saturationWarning);
    if (nAgcRet != 0) 
	{
        printf("WebRtcAgc_Process failed during filtering the last data block. %d\n", nAgcRet);
		free(data_out);
        return -1;
    }
    memcpy(&data_in[samples-remained_samples], &data_out[samples-remained_samples], remained_samples * sizeof(int16_t));
    data_in += samples;

	free(data_out);
    return 1;
}
#ifdef  AGC_SELF_TEST
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define AGC_DATA_LEN    160

int main(int argc, char *argv[])
{
	char in_file[256];
	char out_file[512];
    uint32_t     sample_rate  = 0;
    int  opt;
	int  gain = 6, targetDb = 3;
	int16_t  in_buf[AGC_DATA_LEN];
	int16_t  *data_in  = &in_buf[0];
	FILE *fp_in, *fp_out;
	int  ret, samples_count;
	
    while ((opt = getopt(argc, argv, "f:s:g:t:h")) != -1) 
	{
        switch (opt) {
			case 's':
				sample_rate = atoi(optarg); 
				printf("sample_rate=%d !!! \n", sample_rate);
			break;
			
			case 't':
				targetDb = atoi(optarg); 
				printf("targetDb=%d \n", targetDb);
			break;
			
			case 'g':
				gain = atoi(optarg); 
				printf("gain =%d \n", gain );
			break;

			case 'f':
				strcpy(in_file, optarg); 
				printf("intput file: %s \n", in_file);
			break;
		}
	}
	
	sprintf(out_file, "%s_%d_%d.pcm", in_file, gain, targetDb);

	fp_in  = fopen(in_file, "r");
	if(!fp_in)
	{
		return 0;
	}
	fp_out = fopen(out_file, "w+");
	if(!fp_out)
	{
		fclose(fp_in);
		return 0;
	}
	
	audio_agc_init(gain, targetDb, sample_rate);

	ret = 1;
	samples_count = 160;
	while(ret)
	{
		ret = fread(data_in, 1, samples_count * 2, fp_in);
		if(ret > 0)
		{
			audio_agc_proc(data_in, sample_rate, samples_count);
			fwrite(data_in, 1, samples_count * 2, fp_out);		
		}
	}
	
	audio_agc_free();
	
	fclose(fp_in);
	fclose(fp_out);
	
	return 0;
}
#endif
