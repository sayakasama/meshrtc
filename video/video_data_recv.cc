#include <string>

#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"
#include "rtp_session.h"
#include "h264/h264.h"

#include "cjson.h"
#include "cfg.h"
#include "av_api.h"
#include "../av_cfg.h"

#include "video_data_recv.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static av_params_t encoder_av_params;
static int   max_seconds_of_gop = 10;
static int   min_fps            = 20;
static int   max_fps            = 30;
static int   kbps               = 1000;

// 0:质量优先 1:流畅优先 值的获取在rtc_main.cc周期性更新
extern int   g_rcmode;
//////////////////////////////////////////////////////////////////////////////////////////////////////////
#define  ENCODED_DATA_LIST_NUM  30  // too big, too delays

typedef enum enum_watermark {
	WM_HIGH,
	WM_MIDDLE,
	WM_LOW
}watermark_e;

int  fps_scale_th_high  = 25; // ENCODED_DATA_LIST_NUM/2
int  fps_scale_th_low   = 20; //  ENCODED_DATA_LIST_NUM/6

int  num_in_data_queue();
int  watermark_of_data_queue();
int  rcmode_params_set(av_video_cfg_t *av_cfg);
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static int  max_size   = MAX_H264_FRAME_SIZE;//1 * 1024 * 1024;

char frag_head_flag[4] = {0,0,0,1};
#define  FRAG_HEAD_LEN  4
//////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct tag_encoded_data {
	char *buf;
	int  len;
}encoded_data_t;

static  encoded_data_t  encoded_data_list[ENCODED_DATA_LIST_NUM] = {0};
static  int encoded_data_head = 0;
static  int encoded_data_tail = 0;

static int encoded_data_save_count = 0;
static int encoded_data_get_count  = 0;
static int encoded_data_trigger_count  = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void *video_packet_recv(void *data);
int  encoder_fps_adjust(int fps);
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static  int      av_rtp_session_id = -1;
#ifndef MESH_DJI
static int default_peer_port  = DEFAULT_RTP_VIDEO_MODULE_PORT;
static int default_local_port = DEFAULT_RTP_VIDEO_MODULE_PORT+1;
#else
static int default_peer_port  = RTP_VIDEO_PORT_FOR_MESH_DJI;
static int default_local_port = RTP_VIDEO_PORT_FOR_MESH_DJI + 1;
#endif
static int data_type          = VIDEO_TYPE_H264_DATA; 

static  int data_recv_init_flag  = 0;		
static  int av_cfg_init_flag     = 0;		
static  int first_frag_get       = 1;

static  int   video_data_recv_debug = 0;
static  FILE *fp_save               = NULL;
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int   video_recv_start(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port)
{
	int loop = 0;
	encoded_data_t *encoded_data;
	// av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;
	
	//从本地读取视频配置作为基准
	av_video_cfg_get(&encoder_av_params);
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "recv start...peer_port=%d, local_port=%d", peer_port, local_port);
	
	for(loop=0; loop < ENCODED_DATA_LIST_NUM; loop++)
	{
		encoded_data      = &encoded_data_list[loop];
		encoded_data->buf = (char *)malloc(max_size);
		if(!encoded_data->buf)
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " frag_data memccpy alloc  failed!!! size=%d index=%d", max_size, loop);			
			return -1;
		}
		encoded_data->len = 0;
	}
		
	if(0 == peer_port)  peer_port  = default_peer_port;
	if(0 == local_port) local_port = default_local_port;
	// create video recv thread. local_ip_addr is null to use any valid addr. we don't send video to peer !
	av_rtp_session_id = rtp_session_thread_start("video_data_recv", (const char *)NULL, peer_port, NULL, local_port, data_type);			
	if(av_rtp_session_id < 0)
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " start video_data_recv thread failed!!!");
		return -1;
	}
	
	data_recv_init_flag = 1;

	// create video save thread
	pthread_t tidRtp;
	int ret = pthread_create(&tidRtp, NULL, video_packet_recv, NULL);
	if(ret != 0)
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " video_packet_recv thread create  failed!!! errno=%d %s", errno, strerror(errno));		
		return -1;
	}

	pthread_detach(tidRtp);	

	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "recv start ok!!!");

	if(video_data_recv_debug)
	{
		fp_save = fopen("./video_from_av.264", "w+");
		if(!fp_save)
		{
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " open file to save h264 failed!!! errno=%d %s", errno, strerror(errno));
		}
	}
	
	return 1;
}				

//////////////////////////////////////////////////////////////////////////////////////////////////////////		
void video_recv_stop(int reason)
{
	encoded_data_t  *encoded_data;
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "video_recv_stop stop reason=%d!!! data_recv_init_flag=%d ", reason, data_recv_init_flag);
	
	av_cfg_init_flag = 0;
	first_frag_get   = 1;
	
	if(0 == data_recv_init_flag)
	{
		return;
	}
	
	data_recv_init_flag     = 0;	
	
	encoded_data_save_count = 0;
	encoded_data_get_count  = 0;
	encoded_data_trigger_count = 0;

	encoded_data_head       = 0;
	encoded_data_tail       = 0;
	
	for(int loop=0; loop < ENCODED_DATA_LIST_NUM; loop++)
	{
		encoded_data = &encoded_data_list[loop];
		if(encoded_data->buf)
		{
			free(encoded_data->buf);
			encoded_data->buf = NULL;
		}
	}

	if(av_rtp_session_id != -1)
	{
		rtp_session_thread_stop(av_rtp_session_id);
		av_rtp_session_id = -1;
	}	
		
	if(video_data_recv_debug)
	{
		if(fp_save)
		{
			fclose(fp_save);
			fp_save = NULL;
		}
	}			
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoded_data_save(char *data, int len, int pkt_num)
{
	av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;
	encoded_data_t  *encoded_data;
	int real_fps                 = 0;
	static int save_overflow     = 0;
	static int flow_ctrl_discard = 0;
	static int flow_ctrl_flag    = 0;
	static int key_frame_dist    = 0;
	static int recv_data_bytes   = 0;
	static u_int64_t last_save_time = 0;
	int pkt_type, ret;
	
	if(video_data_recv_debug)
	{
		if(fp_save)
		{
			fwrite(data, 1, len, fp_save);
		}
	}

#if 1 //def LINUX_X86
	pkt_type = data[FRAG_HEAD_LEN]; 		
	if((pkt_type != SPS_FRAG) && (pkt_type != PPS_FRAG))
	{
		key_frame_dist++;
		if(flow_ctrl_flag)
		{
			flow_ctrl_discard++;
			return 0;
		}		
		
		ret = watermark_of_data_queue();
		if(ret == WM_HIGH) 
		{
			if(key_frame_dist > (video_cfg->gop+1)/2)
			{
				if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE)
				{
					flow_ctrl_flag = 1;
					MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " flow_ctrl start 3: gop=%d key_frame_dist=%d buf_in_queue=%d flow_ctrl_discard=%d", video_cfg->gop, key_frame_dist, num_in_data_queue(), flow_ctrl_discard);
				}
				else
				{
					flow_ctrl_flag = 0;
				}
			}
		} 
		else if(ret == WM_MIDDLE)  
		{
			if(key_frame_dist > (video_cfg->gop+1) * 4/5)
			{
				if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE)
				{
					flow_ctrl_flag = 1;
					MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " flow_ctrl start 1: gop=%d key_frame_dist=%d buf_in_queue=%d flow_ctrl_discard=%d", video_cfg->gop, key_frame_dist, num_in_data_queue(), flow_ctrl_discard);
				}
				else
				{
					flow_ctrl_flag = 0;
				}
				
			}
		}
	} else {
		if(flow_ctrl_flag)
		{
			flow_ctrl_flag = 0; // discard until a key frame
			if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE){
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " flow_ctrl stop on key frame: gop=%d key_frame_dist=%d flow_ctrl_discard=%d", video_cfg->gop, key_frame_dist, flow_ctrl_discard);
			}
		}
		key_frame_dist = 0;
	}
#endif		
	
	if((pkt_type != SPS_FRAG) && (pkt_type != PPS_FRAG))
	{
		ret = ((encoded_data_head + 2) % ENCODED_DATA_LIST_NUM == encoded_data_tail); // reserve a buf for key frame
	} else {
		ret = ((encoded_data_head + 1) % ENCODED_DATA_LIST_NUM == encoded_data_tail); // no buf for key frame
	}
	if(ret)
	{
		if(!flow_ctrl_flag && (key_frame_dist > (video_cfg->gop+1)/2))
		{
			if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE){
				flow_ctrl_flag = 1; // discard next p frame to avoid mosaic
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " flow_ctrl start 3: gop=%d key_frame_dist=%d buf_in_queue=%d flow_ctrl_discard=%d", video_cfg->gop, key_frame_dist, num_in_data_queue(), flow_ctrl_discard);
			}
			else
			{
				flow_ctrl_flag = 0;
			}
		}
		save_overflow++;
		if(save_overflow % 10 == 0)
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "overflow (pkt_type=%d)!!! encoded_data_save_count=%d encoded_data_trigger_count=%d encoded_data_get_count=%d save_overflow=%d", \
														pkt_type, encoded_data_save_count, encoded_data_trigger_count, encoded_data_get_count, save_overflow);
		return -1; // buf overflow
	}
	
	encoded_data = &encoded_data_list[encoded_data_head];
	memcpy(encoded_data->buf, data, len);
	encoded_data->len = len;
	
	encoded_data_save_count++;
	if(encoded_data_save_count == 0)
	{
		last_save_time = sys_time_get();
	}
	
	recv_data_bytes += len;
	if(encoded_data_save_count % 20 == 0)
	{
		u_int64_t now  = sys_time_get();
		real_fps       = (20 * 1000) / (now - last_save_time); // 500 pkt recv in 1000 ms
		ret            = recv_data_bytes * 8 / (now - last_save_time);
		last_save_time = now;
		recv_data_bytes= 0;
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "++++++ real_fps=%d [encoded_data_save_count=%d] head=%d len=%d [pkt_num=%d][code rate=%d kbps] time=%ld ++++++", \
										real_fps, encoded_data_save_count, encoded_data_head, len, pkt_num, ret, now);
	}
	encoded_data_head = (encoded_data_head + 1) % ENCODED_DATA_LIST_NUM;
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
void  encoded_data_clean()
{
	int tmp;
	
	tmp = (encoded_data_head + ENCODED_DATA_LIST_NUM - encoded_data_tail) % ENCODED_DATA_LIST_NUM;
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "init: discard %d video frame in queue", tmp);
	
	encoded_data_head = encoded_data_tail;

	return ;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoded_data_get(char *data, int *len, int *have_encoded_num)
{
	encoded_data_t  *encoded_data;
	
	*len = 0;
	if(!data_recv_init_flag)
	{
		return 0; 
	}
	
	if(encoded_data_head == encoded_data_tail)
	{
		return 0; 
	}
	
	encoded_data = &encoded_data_list[encoded_data_tail];
	memcpy(data , encoded_data->buf, encoded_data->len); 
	*len  = encoded_data->len;

	if(first_frag_get)
	{
		first_frag_get    = 0;
		*have_encoded_num = 0;
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " video frag get %d: save=%d len=%d [num in queue:%d]", first_frag_get, encoded_data_save_count, *len, encoded_data_trigger());
	}

	encoded_data_get_count++;
	if(encoded_data_get_count % 1000 == 0)
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " [save=%d get=%d encoded=%d] tail=%d len=%d time=%lu", \
						encoded_data_save_count, encoded_data_get_count, *have_encoded_num, encoded_data_tail, *len, sys_time_get());
	}
	
	encoded_data_tail = (encoded_data_tail + 1) % ENCODED_DATA_LIST_NUM;	
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int num_in_data_queue()
{
	int ret;
	ret = (encoded_data_head + ENCODED_DATA_LIST_NUM - encoded_data_tail) % ENCODED_DATA_LIST_NUM;
	return ret;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int watermark_of_data_queue()
{
	int ret;
	
	ret = (encoded_data_head + ENCODED_DATA_LIST_NUM - encoded_data_tail) % ENCODED_DATA_LIST_NUM;
	if(ret >= fps_scale_th_high) //(ENCODED_DATA_LIST_NUM * 3/5))
	{
		return WM_HIGH;
	} 
	else if(ret >= fps_scale_th_low) //(ENCODED_DATA_LIST_NUM * 1/5)) 
	{
		return WM_MIDDLE;
	}
	else
	{
		return WM_LOW;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoded_init_ok()
{
	return data_recv_init_flag;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoded_data_trigger()
{
    if(!data_recv_init_flag)
	{
		return 0;
	}
	
	if(encoded_data_head == encoded_data_tail)	
	{
		return 0; 
	}
	
	encoded_data_trigger_count++;
	
	return ((encoded_data_head + ENCODED_DATA_LIST_NUM) - encoded_data_tail) % ENCODED_DATA_LIST_NUM;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoded_data_ready()
{	
    if(!data_recv_init_flag)
	{
		return 0;
	}
	
	if(encoded_data_head == encoded_data_tail)
	{
		return 0; 
	}
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
void *video_packet_recv(void *data)
{			
	int size, start_flag, pkt_type;
	int frag_pkt_num       = 0;
	int first_gop          = 0; // some mosair . get any video to trigger video encoder init
	int idr_start          = 0;
	int packet_len_max     = 2048;
	int pkt_discard_before_idr  = 0;
	int frag_discard_before_idr = 0;
	
    char    *frag_data, *packet_data;
	int      frag_data_offset;

	packet_data = (char *)malloc(packet_len_max);
	if(!packet_data)
	{
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " packet_data memccpy alloc  failed!!! size=%d", packet_len_max);
		return NULL;
	}

	frag_data   = (char  *)malloc(max_size);
	frag_data_offset = 0;
	if(!frag_data)
	{
		free(packet_data);
		MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " frag_data memccpy alloc  failed!!! size=%d", max_size);		
		return NULL;
	}
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " thread start to recv video data");		
	start_flag = 0;
	while(data_recv_init_flag)
	{		
		size = 0;
		rtp_session_data_dequeue(av_rtp_session_id, (char *)packet_data, &size, max_size);
		if(size == 0)
		{
			sleep_ms(5);
			continue;
		}

		frag_pkt_num++;
		// check first IDR video frag
		if(first_gop == 0)
		{
			if(0 != memcmp(packet_data, frag_head_flag, FRAG_HEAD_LEN))
			{
				pkt_discard_before_idr++; // not frag head 
				continue;
			}
			// discard video frame before first gop. these video frame can't be decoded
			pkt_type = packet_data[FRAG_HEAD_LEN]; 		
			if((pkt_type == SPS_FRAG) || (pkt_type == PPS_FRAG))
			{
				first_gop = 1;
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "+++++++ recv first gop start. frame type=0x%02x . pkt_discard_before_idr=%d frag_discard_before_idr=%d ++++++++++\n", \
										pkt_type, pkt_discard_before_idr, frag_discard_before_idr);
			} else {
				frag_discard_before_idr++;
				pkt_discard_before_idr++;
				continue; 
			}
		}
		
		if(0 != memcmp(packet_data, frag_head_flag, FRAG_HEAD_LEN))
		{
			if((frag_data_offset + size) > max_size)
			{
				// discard too big frag
				// refind a new frag with start flag
				frag_data_offset = 0;
				frag_pkt_num     = 0;			
				start_flag       = 0; 
				continue; 
			}
			
			// add data into frag_data tail
			memcpy(frag_data + frag_data_offset, packet_data, size);
			frag_data_offset += size;
			continue;
		}

		if((start_flag == 0) || (0 == frag_data_offset))
		{
			//discard the prev frag that is not with frag_head_flag
			start_flag       = 1;
			frag_data_offset = size;
			frag_pkt_num     = 1;
			// start a new frag by current pkt data
			memcpy(frag_data, packet_data, size);
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "save pps for later I frame. this frame size=%d ...............", size);
			continue; 
		}
		
		// If last frag is a pps frame. add new frame on the tail to combine a whole frame with pps & sps & i-frame
		pkt_type = frag_data[FRAG_HEAD_LEN];
		if((pkt_type == PPS_FRAG) && (0 == idr_start) && (frag_data_offset < 1024))
		{
			idr_start = 1;
			// prev frag is a small sps & pps frag without video frag data 
			// add current video frag data into prev sps&pps frag_data tail to construct a compact IDR frame
			memcpy(frag_data + frag_data_offset, packet_data, size);
			frag_data_offset += size;
			MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "combine a IDR frame len=%d encoded_data_get_count=%d encoded_data_save_count=%d ...\n", \
								frag_data_offset, encoded_data_get_count, encoded_data_save_count);
			continue;
		}

		idr_start = 0; // waiting for next idr frame 
		// save prev video frag data		
		encoded_data_save(frag_data, frag_data_offset, frag_pkt_num);
		
		// start a new frag
		start_flag       = 1;		
		frag_data_offset = size;
		frag_pkt_num     = 1;
		memcpy(frag_data, packet_data, size);
	}

	free(packet_data);
	free(frag_data);

	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_ERR, " thread stop to recv video data");		
	
	return NULL;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  video_osd_cfg_update(av_osd_cfg_t *osd_new)
{
	av_osd_cfg_t       *osd   = &encoder_av_params.osd_cfg;
	
	memcpy(osd, osd_new, sizeof(av_osd_cfg_t));
	
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
void encoder_wh_get(int *w, int *h)
{
	av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;

	*w = video_cfg->w;
	*h = video_cfg->h;
	
	return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
void encoder_wh_set(int w, int h)
{
	av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;

	video_cfg->w = w;
	video_cfg->h = h;
	
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " encoder_wh_set: w=%d h=%d ", w, h);
	
	return;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void  bps_map_video_wh(int bps, int *w, int *h,int *target_bps,int bps_set)
{
	if(g_rcmode == 1) //流畅优先模式下，1080P要求码率不超过1500k，720p要求码率不高于768k
	{
		if(bps > bps_set)
		{
			bps = bps_set; //首先流畅模式下bps配置不得超过配置bps
		}
		if(bps <= 768 * kbps)
		{
			*target_bps = bps;
			*w = 640; // 480p
			*h = 480;
			return;
		}
		else if(bps >= 768 * kbps && bps < 1200 * kbps)
		{
			*target_bps = bps;
			*w = 1280; // 1080p
			*h = 720;
			return;
		}
		else
		{
			*target_bps = 1200 * kbps;
			*w = 1920; // 1080P
			*h = 1080;
			return;
		}
	}
	else
	{
		*target_bps = bps_set;
	}
	if(*target_bps >= 10000 * kbps)
	{
		if(*w >= 3840 || *h >= 2160)
		{
			*w = 3840; // 4k
			*h = 2160;
		}
	} 	
	else if(*target_bps >= 6000 * kbps)
	{
		if(*w >= 2560 || *h >= 1440)
		{
			*w = 2560; // 2k
			*h = 1440;
		}
	} 		
	else if(*target_bps >= 3000 * kbps)
	{
		if(*w >= 1920 || *h >= 1080)
		{
			*w = 1920; // high res
			*h = 1080;
		}
	} 		
	else if(*target_bps >= 1000 * kbps)/*if(bps > 900 * kbps)*/
	{
		*w = 1280; // standard res. limit min resulotion to 1280*720
		*h = 720;
	}
	else
	{
		*w = 640;
		*h = 480;
	} /*	
	else if(bps > 500 * kbps)
	{
		*w = 704; // D1
		*h = 576;
	} 
	else if(bps > 300 * kbps)
	{
		*w = 640;
		*h = 480;
	} 	
	else if(bps > 200 * kbps)
	{
		*w = 480;
		*h = 320;
	} 		
	else
	{   // CIF
		*w = 352;
		*h = 288;		
	}*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//2025/2/25 wangyun :码率调整逻辑 分为通常模式和流畅优先模式。
//首先获取当前系统配置码率，根据模式计算目标码率：通常模式：目标码率不高于系统配置码率即可；流畅优先模式：720p下不超过768kbps，1080p下不超过1500kbps，不支持其他分辨率。
//根据目标码率阈值配置分辨率
//通过分辨率，帧率和目标码率计算QP值
int  encoder_cfg_update(int codec, int w, int h, int fps, int gop, int bps)
{
	av_video_cfg_t *video_cfg = &encoder_av_params.video_cfg;
	int w_limit, h_limit;
	int target_bps = 0;
	int bps_sys_set = 0;
	int qp = 30;
	int seconds, ret;
	static int cfg_change_discard    = 0;
	static u_int64_t fps_adjust_time = 0;
	static int bps_last              = 0;
	
	if((fps == 0) || (w == 0)  || (h == 0)  || (bps == 0)  || (gop == 0) )
	{
		return 0;
	}
	
	ret = watermark_of_data_queue();
	if((ret == WM_HIGH) && (bps > bps_last))
	{
		// MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "NOT update cfg of av module: (target codec=%d w=%d h=%d bps=%d gop=%d) & (current w=%d h=%d) [watermark_of_data_queue:%d] bps_last=%d ",\
		// 							codec, w, h, bps, gop, video_cfg->w, video_cfg->h, ret, bps_last); 
		return 0;
	}
	// code rates limit the video resulotion
	cfg_bps_get(&bps_sys_set); //获取系统配置的bps值
	bps_map_video_wh(bps, &w, &h, &target_bps,bps_sys_set);
	av_wh_max_get(&w_limit, &h_limit); // limit av module resolution
	if((w > w_limit) || (h > h_limit))
	{
		w = w_limit;
		h = h_limit;
	}
	
	fps = encoder_fps_adjust(video_cfg->fps);
	
	av_cfg_init_flag = 1;

	seconds = video_cfg->gop / fps;
	if(seconds > max_seconds_of_gop) 
	{
		gop = max_seconds_of_gop * fps; // limit gop max value. shangtao 20240401
	}else{
		gop = video_cfg->gop;
	}
	if(seconds == 0)
	{
		seconds = 1;
	}
	
	if(    (video_cfg->codec == codec) && (video_cfg->gop == gop) && (video_cfg->fps == fps)
		&& (video_cfg->w == w) && (video_cfg->h == h) && (video_cfg->code_rate == bps) )
	{
		return 1; // no change
	}
	
	if( (video_cfg->codec == codec) && (video_cfg->w == w) && (video_cfg->h == h) )
	{
		float diff_fps = (float)abs(video_cfg->fps - fps) / (float)fps;
		float diff_bps = (float)abs(video_cfg->code_rate - bps) / (float)bps;		
		if((diff_fps < 0.05) && (diff_bps < 0.1))
		{
			cfg_change_discard++;
			if(cfg_change_discard % 10 == 0)
			{
				MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "small fps diff_fps=%f diff_bps=%f & not change cfg of av module....current fps=%d bps=%d\n target codec=%d w=%d h=%d fps=%d bps=%d gop=%d", \
								diff_fps, diff_bps, video_cfg->fps, video_cfg->code_rate, codec, w, h, fps, target_bps, gop); 
			}
			return 1; // no change
		}
	}	
	
	if((sys_time_get() - fps_adjust_time) < 2000)
	{
		// MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "not update video cfg frequently.");
		return 1;
	}
	fps_adjust_time = sys_time_get();
	
	video_cfg->codec  = codec;
	video_cfg->gop    = gop;
	video_cfg->fps    = fps;
	video_cfg->w      = w; 
	video_cfg->h      = h;
	video_cfg->code_rate = target_bps;

	bps_last          = target_bps;
	if(target_bps < 500 * kbps)
	{
		if(video_cfg->fps > (max_fps*2)/3)	
		{
			video_cfg->fps = (max_fps*2)/3;
		}
	}
	if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE)
	{
		video_cfg->fps = 15;
	}
	qp = rcmode_params_set(video_cfg); //根据码率控制策略控制qp
	// set video params to av module. 
	video_cfg_set((char *)&encoder_av_params, sizeof(encoder_av_params));

	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "update cfg of av module....codec=%d w=%d h=%d fps=%d bps=%d gop=%d qp_min=%d [video frag in queue:%d]",video_cfg->codec,video_cfg->w,video_cfg->h,video_cfg->fps, target_bps,video_cfg->gop,qp,encoded_data_trigger()); 
	return 1;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoder_force_idr()
{
	// It not used and maybe deprecated in future. 20240328
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " do nothing now !!! we will setup IDR in av module...."); 
	return 1;	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoder_frame_rate_set(int fps)
{
	//. setup frame rate to av module. It not used and maybe deprecated in future.
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " do nothing now...."); 
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoder_bps_set(int bps)
{
	// It not used and maybe deprecated in future. 20240328
	MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, " do nothing now !!!!! we will setup the real value in av modules"); 
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int fps_range_check(int fps)
{
	if(fps < min_fps)
	{
		fps = min_fps; 
	} 
	else if(fps > max_fps) 
	{
		fps = max_fps;
	}
	
	return fps;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////		
int  encoder_fps_adjust(int fps_in)
{
	static u_int64_t fps_adjust_time = 0;
	u_int64_t  now;		
	int fps, ret, diff;

	ret = watermark_of_data_queue();
	if(ret == WM_MIDDLE) 
	{
		return fps_range_check(fps_in);
	}

	now  = sys_time_get();
	diff = (now - fps_adjust_time);
	
	//MESH_LOG_PRINT(CONDUCTOR_MODULE_NAME, LOG_INFO, "target_fps=%d calc fps of av module....current fps=%d scale_down=%d  time=%d", target_fps, video_cfg->fps, scale_down, ret); 
	if(ret == WM_HIGH)
	{
		if(diff < 2000) // scale down quickly at 3000ms inteval
		{
			return fps_range_check(fps_in);
		}
		fps = fps_in * 2/3; // 0.66		
	} else {
		if(diff < 4 * 1000) // scale up slowly at 6000ms inteval
		{
			return fps_range_check(fps_in);
		}
		fps = fps_in * 11/10; // 1.1
		if(fps == fps_in)
		{
			fps = fps + 1;
		}		
	}

	return fps_range_check(fps);
}

int rcmode_params_set(av_video_cfg_t *av_cfg)
{
	int qp_temp = 0;
	if(g_rcmode == RC_MODE_NORMAL_MODE)
	{
		qp_temp = qp_calculate(av_cfg) - 2;
		av_cfg->qp = qp_temp >= 12 ? qp_temp : 12; // 确保最小QP不低于12
	}
	else if(g_rcmode == RC_MODE_FLUENCY_FIRST_MODE)
	{
		//qp_temp = qp_calculate(av_cfg);
		//av_cfg->qp = qp_temp <= MAX_QP_MIN_VALUE ? qp_temp : MAX_QP_MIN_VALUE;
		if(av_cfg->w == 1920)
		{
			av_cfg->qp = 26;
			qp_temp = 26;
		}
		else if(av_cfg->w == 1280)
		{
			av_cfg->qp = 25;
			qp_temp = 25;
		}
		else if(av_cfg->w == 640)
		{
			av_cfg->qp = 19;
			qp_temp = 19;
		}
	}
	return qp_temp;
}