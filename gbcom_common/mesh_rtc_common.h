#ifndef __MESH_RTC_COMMON_H__
#define __MESH_RTC_COMMON_H__

///////////////////// mesh dev type definition ///////////////////////////////

#define  MESH_DEV_T_Z800AG 0
#define  MESH_DEV_T_Z800AH 1
#define  MESH_DEV_T_Z800BG 2
#define  MESH_DEV_T_Z800BH 3
#define  MESH_DEV_T_9300M  4
#define  MESH_DEV_T_XM2300 5  //配合dji无人机使用
#define  MESH_DEV_T_XR9700 6
#define  MESH_DEV_T_Z900   7

#define  MESH_DEV_BASE_STATION 0x10
//////////////////////////////////////////////////////////////////////////

#define  MESH_RTC_OK   1
#define  MESH_RTC_FAIL -1

#define URL_MAX_LEN       1024

#define FILE_NAME_MAX_LEN 1024
#define IP_STR_MAX_LEN    16    //123.456.789.123

#define  GSOAP_DEFAULT_PORT  4567
#define  GSOAP_MAX_RET_SIZE  4096
//////////////////////////////// ipc //////////////////////////////////////////
#define MODULE_NAME_MAX_LEN     64

#define MAIN_MODULE_NAME         "rtc_main"
#define PTT_MODULE_NAME          "ptt"
#define AV_ADAPTER_MODULE_NAME   "av_adapter"
#define LOCAL_SERVER_MODULE_NAME "local_rtc_server"
#define RTC_CONN_MODULE_NAME     "rtc_conn"
#define CONN_CTRL_MODULE_NAME    "conn_ctrl"
#define MESH_UI_MODULE_NAME      "mesh_ui"
#define MESH_UI_PHOTO_MODULE_NAME "mesh_ui_photo"
#define COMMON_MODULE_NAME       "common"
#define AUDIO_DEV_MODULE_NAME    "audio_dev"
#define VIDEO_DEV_MODULE_NAME    "video_dev"
#define CONDUCTOR_MODULE_NAME    "conductor"
#define PC_MESH_MODULE_NAME      "pc_mesh"
#define PC_CLIENT_MODULE_NAME    "pc_client"
#define RPC_SERVER_MODULE_NAME    "rpc_server"
#define RPC_CLIENT_MODULE_NAME    "rpc_client"
#define AV_CFG_MODULE_NAME        "av_cfg"
#define AV_AUDIO_MODULE_NAME      "av_audio"
#define AV_VIDEO_MODULE_NAME      "av_video"
#define AV_CTRL_MODULE_NAME       "av_ctrl"
#define MIXER_MODULE_NAME         "audio_mixer"
#define WARN_MODULE_NAME          "warn_audio"
#define MESH_MGT_MODULE_NAME      "mesh_mgt"
#define HP_ADAPTER_MODULE_NAME    "hp_adapter"
#define GB28181_MODULE_NAME       "gb28181"
#define HP_VIDEO_MODULE_NAME      "hp_video"
#define H264_DEOCDE_MODULE_NAME   "h264_decode"

#define MESH_UI_ALARM_AREA_MODULE_NAME "mesh_ui_alarm_area"

#define IPC_MSG_MAX_SIZE  1024
typedef struct tag_msg_buf
{
    long mtype;
    char msg_content[IPC_MSG_MAX_SIZE];
}msg_buf_t;

#define COMMON_MSG_STR_LEN  32
typedef struct tag_common_msg_head 
{
	unsigned int   src_module_id;
	unsigned int   dst_module_id;
	unsigned int   msg_type;
	unsigned short msg_len;
	unsigned char  msg_sn; // default is 0, increase one if same msg resend
	unsigned char  rsv; 
	char           msg_str[COMMON_MSG_STR_LEN];
	char           msg_data[0];
}common_msg_head_t;
/////////////////////////////// port definition  ////////////////////////////////

// RTP/UDP
#define RTP_UDP_PORT_BASE           30000
#define DEFAULT_RTP_UNICAST_PORT    65530

#define DEFAULT_RTP_MULTICAST_PORT  (RTP_UDP_PORT_BASE)
#define DEFAULT_RTP_PTT_PORT        (DEFAULT_RTP_MULTICAST_PORT+2)
#define DEFAULT_RTP_AV_MODULE_PORT  (DEFAULT_RTP_PTT_PORT+2)       // not used
#define DEFAULT_RTP_AV_ADAPTER_PORT (DEFAULT_RTP_AV_MODULE_PORT+2)
#define DEFAULT_RTP_BLE_PORT        (DEFAULT_RTP_AV_ADAPTER_PORT+2)
#define DEFAULT_RTP_WARN_PORT       (DEFAULT_RTP_BLE_PORT+2)

#define DEFAULT_RTP_MIXER_PORT      (DEFAULT_RTP_WARN_PORT+2)

#define DEFAULT_RTP_VIDEO_MODULE_PORT  (DEFAULT_RTP_MIXER_PORT+2)
#define DEFAULT_RTP_AUDIO_MODULE_PORT  (DEFAULT_RTP_VIDEO_MODULE_PORT+2)
#define DEFAULT_RTP_HAND_ADUIO_MODULE_PORT (DEFAULT_RTP_AUDIO_MODULE_PORT+2)

#define DEFAULT_RTP_HP_VIDEO_PORT     (DEFAULT_RTP_HAND_ADUIO_MODULE_PORT+2)
#define DEFAULT_RTP_H264_DECODE_PORT  (DEFAULT_RTP_HP_VIDEO_PORT+2)

#define DEFAULT_RTP_GB28181_PORT       (DEFAULT_RTP_H264_DECODE_PORT + 2) 
#define DEFAULT_RTP_GB28181_AUDIO_PORT (DEFAULT_RTP_GB28181_PORT + 2)

#define RTP_VIDEO_PORT_FOR_MESH_DJI  31995 // for DJI mesh video, use this port

//http
#define DEFAULT_LOCAL_RTC_SERVER_PORT  8082

#define AV_MODULE_DEFAULT_ADDR      "169.167.123.113"
#define RTP_MULTICAST_ADDR          "224.0.1.1"
#define MESH_BTS_EXTERNAL_IF        "br0:1"
#define MESH_BTS_INTERNAL_IF        "br0"
#define MESH_TERMINAL_EXTERNAL_IF   "br0"
#define MESH_MAINSTAT_EXTERNAL_IF   "eth-0"
#define MESH_TERMINAL_INTERNAL_IF   "br0:1"
//#define RTP_MULTICAST_IF            MESH_EXTERNAL_IF

#define IPC_MSG_CFG_VIDEO           "cfg_video:"
#define IPC_MSG_CFG_AUDIO           "cfg_audio:"
#define IPC_MSG_CFG_CHANN           "cfg_chann:"
#define IPC_MSG_PTT_STATUS          "ptt_status:"

#define MSG_ADAPTER_ON               "adapter_on"
#define MSG_ADAPTER_OFF              "adapter_off"

#define MSG_GB28181_ON               "gb28181_on"
#define MSG_GB28181_OFF              "gb28181_off"
//////////////////////////////////////////////////////////////////////////
typedef enum enum_osd_position {
	OSD_LEFT_TOP = 1,
	OSD_RIGHT_TOP,
	OSD_LEFT_BOTTOM,
	OSD_RIGHT_BOTTOM
}osd_position_e;

#define  OSD_TEXT_LEN_MAX  256

typedef struct tag_av_osd {
	
	int  time_show;
	char time_format[64];
	int  time_position; 						//时间戳水印位置 1左上 2 右上 3 左下 4 右下
	int  time_text_size;						//时间戳字体大小
	int  time_margin;						//时间戳水印的边距
	int  time_offset_x;
	int  time_offset_y;		
	
	int  title_show; 						//是否显示标题文字
	int  title_position; 						//标题水印位置 1左上 2 右上 3 左下 4 右下
	int  title_text_size;						//标题字体大小
	int  title_margin;						//标题水印的边距
	int  text_offset_x;
	int  text_offset_y;			
	
	int  text_len;
	char title_text[OSD_TEXT_LEN_MAX];                        // title : chinese text utf-8 encode
	
	int  gnss_show;
	int  gnss_position; 						//gnss 水印位置 1左上 2 右上 3 左下 4 右下
	int  gnss_text_size;					
	int  gnss_margin;						
	int  gnss_offset_x;
	int  gnss_offset_y;
	
	int  bat_sig_show;    //显示电池电量 信号强度
	int  bat_sig_position;
	int  bat_sig_text_size;
	int  bat_sig_margin;
	int  bat_sig_offset_x;
	int  bat_sig_offset_y;
} __attribute__((packed)) av_osd_cfg_t;

typedef struct tag_audio_cfg {
	unsigned int enable;
	unsigned int samples;
	unsigned int aec;
	unsigned int volume;
} __attribute__((packed)) av_audio_cfg_t;

typedef enum enum_codec_type {
	VIDEO_CODEC_H264 = 2,
	VIDEO_CODEC_H265 = 3,
	VIDEO_CODEC_H266 = 4	
}codec_type_e;

typedef enum enum_eRcMode_type {
	RC_MODE_NORMAL_MODE = 0,
	RC_MODE_FLUENCY_FIRST_MODE = 1
}rcmode_type_e;

typedef struct tag_video_cfg {
	int qp;   // 15 - 48
	int gop;  //  1- 1000	
	int fps;  // 1 - 60
	int code_rate; // bps
	int w;    // 
	int h;
	int codec;	// 2, h264; 3, h265; 4, h266
} __attribute__((packed)) av_video_cfg_t;

typedef struct tag_av_params {
	av_video_cfg_t video_cfg;
	av_osd_cfg_t   osd_cfg;	
} __attribute__((packed)) av_params_t;


#endif