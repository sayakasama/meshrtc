#ifndef  __PCM_RESAMPLE_H__
#define  __PCM_RESAMPLE_H__


#ifdef  __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////
unsigned int resample_s16(const short *input, short *output, unsigned int in_samples_rate, int out_samples_rate, unsigned int in_len, int rsv);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif