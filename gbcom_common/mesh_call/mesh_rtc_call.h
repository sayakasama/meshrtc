#ifndef __MESH_RTC_CALL_CMD_H__
#define __MESH_RTC_CALL_CMD_H__


#ifdef  __cplusplus
extern "C" {
#endif

//////////////// mesh call cmd & status string  ///////////////
//. status ind
#define	MESH_LOGIN_IND		 "mesh_login_ind"        // local login
#define	MESH_LOGIN_FAIL		 "mesh_login_fail"       // local login fail
#define	MESH_LOGIN_BUSY		 "mesh_login_busy"       // local login busy
#define	MESH_LOGOUT_IND		 "mesh_logout_ind"       // local logout
#define	MESH_CALL_FAIL_IND	 "mesh_call_fail_ind"    // remote busy
#define	MESH_CALL_CONN_IND	 "mesh_conn_ind"         // remote connected
#define	MESH_CALL_REJECT_IND "mesh_call_not_allowed" // not allowed
#define MESH_CALL_DISCONN_IND "mesh_call_disconn_ind"
#define MESH_CALL_TIMEOUT_IND "mesh_call_timeout"

// cmd from mesh ui
#define MESH_LOGIN_CMD      "mesh_login"
#define MESH_LOGOUT_CMD     "mesh_logout"
#define	MESH_CALL_START_CMD	"mesh_call"        // remote login & start call connect. 
#define	MESH_CALL_STOP_CMD	"mesh_hungup"      // remote hungup . 
#define MESH_SERVER_SET_CMD "mesh_server_set"

#define SWITCH_RTC_SERVER_CMD "switch_rtc_server"
#define SWITCH_MESH_SERVER_CMD "switch_mesh_server"

// following are not used indeed.
#define	MESH_CALL_ACCEPT_CMD 	"mesh_accept"    
//#define	MESH_CALL_REJECT_CMD 	"mesh_reject"
#define	MESH_CALL_REJECTED_IND 	"mesh_rejected_ind"    // rejected by remote

#define MESH_WARN_MSG_CMD   "mesh_warn_file"

// cmd for photo update
#define MESH_PHOTO_UPDATE   "mesh_photo_update"
#define MESH_PHOTO_MSG_FORMAT "%s %s" 
//////////////// pc call cmd & status string  ///////////////
//. status ind
#define RTC_LOGIN_IND       "rtc_login_ind"     // rtc server login
#define RTC_LOGOUT_IND      "rtc_logout_ind"     // rtc server logout
#define RTC_CONN_IND        "rtc_conn_ind"
#define RTC_DISCONN_IND     "rtc_disconn_ind"
	
// cmd from mesh ui
#define RTC_SERVER_SET_CMD  "rtc_server_set"
#define RTC_LOGIN_CMD       "rtc_login"
#define RTC_LOGOUT_CMD      "rtc_logout"

// cmd for 大疆无人机(大疆无人机推流与XV9200推流复用同一套推流逻辑)
// cmd from XV9200 inner rtc server
#define RTC_HDMI_CALL_CMD         "rtc_hdmi_call"
#define DJI_STREAM_NAME_SET_CMD   "rtc_dji_stream_name_set"
#define RTC_DJI_STREAM_START_CMD  "rtc_dji_stream_start" //未使用
#define RTC_DJI_STREAM_STOP_CMD   "rtc_dji_stream_stop"

// not used in normally.  rtc_media  same as rtc_server.
#define RTC_MEDIA_SET_CMD   "rtc_media_set"  
#define RTC_DISCONN_CMD     "rtc_disconn"
	
////////////// PTT  cmd & status  string //////////////////////	
#define	MESH_PTT_ON			"ptt_chan_on"
#define	MESH_PTT_OFF		"ptt_chan_off"

#define	MESH_PTT_PUSH		"chan_occupy_req"
#define	MESH_PTT_PUSH_OK	"chan_occupy_ind"
#define	MESH_PTT_PUSH_FAIL	"chan_occupy_fail"
#define	MESH_PTT_POP		"chan_rel_req"
//#define	MESH_PTT_POP_OK		"chan_rel_ind"

#define	MESH_PTT_REQ		"chan_req_ind"
#define	MESH_PTT_REL 		"chan_rel_ind"
#define	MESH_PTT_BUSY		"chan_occupied_ind"
#define	MESH_PTT_COLLISION	"chan_colliding_ind"
	

#define	MESH_RTC_SERVER_ONLINE	"rtc_server_online"
#define	MESH_RTC_SERVER_OFFLINE	"rtc_server_offline"

////////////// av cfg cmd string //////////////////////	
//#define AV_CFG_TYPE
// av_cfg_osd, av_cfg_audio, av_cfg_video
#define AV_CFG_TYPE                 "av_cfg_type"

#define AV_OSD_TITLE_SIZE_CMD       "osd_title_size"
#define AV_OSD_TIME_SIZE_CMD        "osd_time_size"
#define AV_OSD_TIME_SET_CMD         "osd_time"
#define AV_OSD_TIME_LOC_CMD         "osd_time_loc" 
#define AV_OSD_TIME_OFFSET_X        "osd_time_offset_x" 
#define AV_OSD_TIME_OFFSET_Y        "osd_time_offset_y"
#define AV_OSD_TEXT_SET_CMD         "osd_text"
#define AV_OSD_TEXT_LOC_CMD         "osd_text_loc" 
#define AV_OSD_TEXT_OFFSET_X        "osd_text_offset_x" 
#define AV_OSD_TEXT_OFFSET_Y        "osd_text_offset_y"
#define AV_OSD_TEXT_FILE_CMD        "osd_text_file"
#define AV_OSD_TEXT_CONTENT_CMD     "osd_text_content"

#define AV_AUDIO_SAMPLE_SET_CMD     "audio_sample"
#define AV_AUDIO_VOLUME_SET_CMD     "audio_volume"
#define AV_AUDIO_CHANN_SET_CMD      "audio_chann"
#define AV_AUDIO_WARN_SET_CMD       "audio_warn"
#define AV_AUDIO_AEC_SET_CMD        "audio_aec"
	
#define AV_VIDEO_QP_SET_CMD         "video_qp"
#define AV_VIDEO_CODE_RATE_SET_CMD  "encode_rate"
#define AV_VIDEO_I_FRAME_SET_CMD    "i_frame"
#define AV_VIDEO_FPS_SET_CMD        "fps"  
#define AV_VIDEO_RESOLUTION_SET_CMD "resolution"
#define AV_VIDEO_RESOLUTION_X       "res_x"
#define AV_VIDEO_RESOLUTION_Y       "res_y"
#define AV_VIDEO_CODEC_SET_CMD      "codec"

#define AV_HDMI_STATUS_FILE         "/tmp/hdmi_status"
#define AV_HDMI_STATUS_FORMAT       "hdmi=%d"

#define AV_PTT_STATUS_FILE          "/tmp/ptt_status"
#define AV_PTT_STATUS_FORMAT        "ptt=%d"

#define AV_AUDIO_EAR_STATUS_FILE    "/tmp/audio_in_status"
#define AV_AUDIO_STATUS_FORMAT       "audio_in=%d"

#define AV_SERVER_STATUS_FILE        "/tmp/av_server_status"
#define AV_SERVER_STATUS_FORMAT      "av_server=%d"

#define RTC_SERVER_CONN_STATUS_FILE   "/tmp/rtc_server_conn"
#define MESH_PEER_ADDR_FILE            "/tmp/mesh_peer_ip"

#define AV_HDMI_RES_FILE             "/tmp/hdmi_res"
#define AV_HDMI_RES_FORMAT           "w=%d h=%d fps=%d"

#define AV_AUDIO_SAMPLES_FILE        "/tmp/audio_sample"
#define AV_PTT_CAP_FILE              "/tmp/ptt_capability"

#define AV_PTT_CAP_SEND      1
#define AV_PTT_CAP_RECV      2

#define AV_AUDIO_CAHNN_HAND_MICROPHONE "hand_microphone"
#define AV_AUDIO_CAHNN_EAR_MICROPHONE  "ear_microphone"
#define AV_AUDIO_CAHNN_AUTO            "auto"
#define AV_AUDIO_CAHNN_BLE             "ble"

#define AV_AUDIO_BLE_STATUS_FILE       "/tmp/audio_ble_status"
#define AV_AUDIO_HAND_STATUS_FILE      "/tmp/audio_hand_status"
#define AV_AUDIO_ALL_OUTPUT_FILE       "/tmp/audio_all_output"
///////////////////////////////////////////////////////
#define INTPUT_CMD_MAX_LEN   256

#define QUIT_CMD_STR   "quit"


int   ptt_capability_get();
void  ptt_capability_set(int send_allow, int recv_allow);

void  audio_output_enable();
void  audio_output_disable();
int   audio_output_allowed();

int   audio_chann_all_output_set(int all_on, int keep_time);
int   audio_chann_all_output_get(int *all_on, u_int64_t *keep_time);
///////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif //__MESH_RTC_CALL_CMD_H__