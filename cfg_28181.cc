#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "cfg.h"
#include "cfg_28181.h"

//////////////////////////////////////////////////////////////
const char *enable_key       = "persist.gb.28181.enable";

const char *peer_realm_key   = "persist.gb.28181.peer_realm";	
const char *peer_sip_id_key  = "persist.gb.28181.peer_sip_id";
const char *peer_ip_key      = "persist.gb.28181.peer_ip";			
const char *peer_port_key    = "persist.gb.28181.peer_port";				

const char *local_realm_key  = "persist.gb.28181.local_realm";	
const char *local_sip_id_key = "persist.gb.28181.local_sip_id";					
const char *local_port_key   = "persist.gb.28181.local_port";
const char *local_istcp_key  = "persist.gb.28181.istcp";
const char *local_av_ch_key  = "persist.gb.28181.local_av_ch";		
	
const char *user_28181_key   = "persist.gb.28181.user";
const char *passwd_28181_key = "persist.gb.28181.passwd";

const char *video_bps_key    = "persist.gb.av.bps";
const char *video_fps_key    = "persist.gb.av.fps";
const char *video_adjust_key = "persist.gb.28181.adjust_time";

const char *sip_expire_time_key  = "persist.gb.28181.expire_time";
const char *sip_hb_period_key    = "persist.gb.28181.hb_period";
const char *sip_reg_period_key   = "persist.gb.28181.reg_period";
const char *sip_max_hb_ot_key    = "persist.gb.28181.max_hb_ot";

const char *video_stream_type_key = "persist.gb.28181.stream_type";

const char *gb28181_server_type_key  =  "persist.gb.28181.server_type";
const char *gb28181_conf_file        =  "/system/bin/gb28181.conf";
//////////////////////////////////////////////////////////////	
static int cfg_peer_port_get(int *port);	
static int cfg_peer_realm_get(char *realm);
static int cfg_peer_id_get(char *sip_id);
static int cfg_peer_ip_get(char *ip_addr);

static int cfg_28181_name_get(char *name, int max_size);
static int cfg_28181_passwd_get(char *passwd, int max_size);

static int cfg_local_port_get(int *port);	
static int cfg_local_realm_get(char *realm);
static int cfg_local_id_get(char *sip_id);
static int cfg_local_istcp_get(int *istcp);
static int cfg_local_av_channel_get(char *local_av_channel);

static int cfg_video_bps_get(int* vbps);
static int cfg_video_fps_get(int* vfps);
static int cfg_video_auto_adjust_time_get(int *auto_ad_time);

static int cfg_sip_expire_time_get(int* expire_time);
static int cfg_sip_hb_period_get(int* hb_period);
static int cfg_sip_reg_period_get(int* reg_period);
static int cfg_max_hb_overtime_get(int* hb_overtime);
////////////////////////////////////////////////////////////////
int gb_28181_cfg_get(gb_28181_cfg_t *gb28181_params)
{
    int ret;
    char buf[64] = {0};

    // 模块是否启用
    ret = get_property(enable_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [0/disable] as default.",enable_key);
		gb28181_params->gb28181_enable = 0;
	}
	else
	{
		gb28181_params->gb28181_enable = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 国标服务器类型
    ret = get_property(gb28181_server_type_key, (char*)gb28181_params->server_type, sizeof(gb28181_params->server_type));
    if(ret <= 0 || strlen((const char*)gb28181_params->server_type) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [wvp] as default.",gb28181_server_type_key);
		strcpy((char*)gb28181_params->server_type, "wvp");
	}

    // SIP服务器域
    ret = get_property(peer_realm_key, (char*)gb28181_params->peer_realm, sizeof(gb28181_params->peer_realm));
    if(ret <= 0 || strlen((const char*)gb28181_params->peer_realm) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [4401020049] as default.",peer_realm_key);
		strcpy((char*)gb28181_params->peer_realm,"4401020049");
	}

    // SIP服务器ID
    ret = get_property(peer_sip_id_key, (char*)gb28181_params->peer_sip_id, sizeof(gb28181_params->peer_sip_id));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [44010200492000000001] as default.", peer_sip_id_key);
        strcpy((char*)gb28181_params->peer_sip_id, "44010200492000000001");
    }

    // SIP服务器地址
    ret = get_property(peer_ip_key, (char*)gb28181_params->peer_ip, sizeof(gb28181_params->peer_ip));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use 10.30.2.8 as default.", peer_ip_key);
        strcpy((char*)gb28181_params->peer_ip,"10.30.2.8");
    }

    // SIP服务器端口
    ret = get_property(peer_port_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [5060] as default.",peer_port_key);
		gb28181_params->peer_port = 5060;
	}
	else
	{
		gb28181_params->peer_port = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // SIP用户域
    ret = get_property(local_realm_key, (char*)gb28181_params->local_realm, sizeof(gb28181_params->local_realm));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use [3401000000] as default.", local_realm_key);
        strcpy((char*)gb28181_params->local_realm,"3401000000");
    }

    // SIP用户认证ID
    ret = get_property(local_sip_id_key, (char*)gb28181_params->local_sip_id, sizeof(gb28181_params->local_sip_id));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use [34010000001180000001] as default.", local_sip_id_key);
        strcpy((char*)gb28181_params->local_sip_id,"34010000001180000001");
    }

    // SIP用户端口
    ret = get_property(local_port_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [5060] as default.",local_port_key);
		gb28181_params->local_port = 5060;
	}
	else{
		gb28181_params->local_port = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 本地视频通道ID
    ret = get_property(local_av_ch_key, (char*)gb28181_params->local_av_channel, sizeof(gb28181_params->local_av_channel));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use [34010000001310000001] as default.", local_av_ch_key);
        strcpy((char*)gb28181_params->local_av_channel,"34010000001310000001");
    }

    // SIP用户名
    ret = get_property(user_28181_key, (char*)gb28181_params->user_name, sizeof(gb28181_params->user_name));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use [Z800A-28181-test] as default.", user_28181_key);
        strcpy((char*)gb28181_params->user_name,"Z800A-28181-test");
    }

    // 密码
    ret = get_property(passwd_28181_key, (char*)gb28181_params->passwd, sizeof(gb28181_params->passwd));
    if(ret <= 0){
        MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed, use [admin123] as default.", passwd_28181_key);
        strcpy((char*)gb28181_params->passwd,"admin123");
    }

    // 信令传输协议(1 tcp;0 udp)
    ret = get_property(local_istcp_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [0/udp] as default.",local_istcp_key);
		gb28181_params->is_tcp = 0;
	}
	else{
		gb28181_params->is_tcp = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 注册有效期(秒)
    ret = get_property(sip_expire_time_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [3600] as default.",sip_expire_time_key);
		gb28181_params->expire_time = 3600;
	}
	else{
		gb28181_params->expire_time = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 心跳周期(秒)
    ret = get_property(sip_hb_period_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [30] as default.",sip_hb_period_key);
		gb28181_params->heartbeat_period = 30;
	}
	else{
		gb28181_params->heartbeat_period = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 注册间隔(秒)
    ret = get_property(sip_reg_period_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [300] as default.",sip_reg_period_key);
		gb28181_params->register_period = 60;
	}
	else{
		gb28181_params->register_period = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 最大心跳超时次数
    ret = get_property(sip_max_hb_ot_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [3] as default.",sip_max_hb_ot_key);
		gb28181_params->max_heartbeat_overtime = 3;
	}
	else{
		gb28181_params->max_heartbeat_overtime = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 视频分辨率自适应调整周期
    ret = get_property(video_adjust_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [0/disable] as default.",video_adjust_key);
		gb28181_params->video_adjust_time = 0;
	}
	else{
		gb28181_params->video_adjust_time = atoi(buf);
	}
	memset(buf, 0, sizeof(buf));

    // 视频流类型
    ret = get_property(video_stream_type_key, buf, sizeof(buf));
	if(ret <= 0 || strlen(buf) == 0){
		MESH_LOG_PRINT(GB28181_MODULE_NAME, LOG_ERR, "Get %s failed,use [0/video+audio double stream] as default.",video_stream_type_key);
		gb28181_params->video_stream_type = 0;
	}
	else{
		gb28181_params->video_stream_type = atoi(buf);
	}

    return 1;
}
////////////////////////////////////////////////////////////////////////////
int gb_28181_cfg_set(gb_28181_cfg_t *gb28181_params)
{
	int ret;
	char buf[32];
	memset(buf,0,32);
	//模块是否启用
	sprintf(buf, "%d", gb28181_params->gb28181_enable);
	set_property(enable_key,buf);
	memset(buf,0,32);
	//国标服务器类型
	set_property(gb28181_server_type_key,(char*)gb28181_params->server_type);
	//SIP 服务器域
	set_property(peer_realm_key,(char*)gb28181_params->peer_realm);  
	//SIP 服务器ID
	set_property(peer_sip_id_key,(char*)gb28181_params->peer_sip_id);
	//SIP 服务器地址
	set_property(peer_ip_key,(char*)gb28181_params->peer_ip);
	//SIP 服务器端口
	sprintf(buf, "%d", gb28181_params->peer_port);
	set_property(peer_port_key,buf);
	memset(buf,0,32);
	//SIP用户域
	set_property(local_realm_key,(char*)gb28181_params->local_realm);
	//SIP用户认证ID 
	set_property(local_sip_id_key,(char*)gb28181_params->local_sip_id);
	//SIP用户端口
	sprintf(buf, "%d", gb28181_params->local_port);
	set_property(local_port_key,buf);
	memset(buf,0,32);
	//信令传输协议(1 tcp;0 udp) 
	sprintf(buf, "%d", gb28181_params->is_tcp);
	set_property(local_istcp_key,buf);
	memset(buf,0,32);
	//本地视频通道ID
	set_property(local_av_ch_key,(char*)gb28181_params->local_av_channel);
	//SIP用户名
	set_property(user_28181_key,(char*)gb28181_params->user_name);
	//密码
	set_property(passwd_28181_key,(char*)gb28181_params->passwd);
	//注册有效期(秒)
	sprintf(buf, "%d", gb28181_params->expire_time);
	set_property(sip_expire_time_key,buf);
	memset(buf,0,32);
	//心跳周期(秒)
	sprintf(buf, "%d", gb28181_params->heartbeat_period);
	set_property(sip_hb_period_key,buf);
	memset(buf,0,32);
	//注册间隔(秒)
	sprintf(buf, "%d", gb28181_params->register_period);
	set_property(sip_reg_period_key,buf);
	memset(buf,0,32);
	//最大心跳超时次数
	sprintf(buf, "%d", gb28181_params->max_heartbeat_overtime);
	set_property(sip_max_hb_ot_key,buf);
	memset(buf,0,32);
	//视频分辨率自适应调整周期(秒)
	sprintf(buf, "%d", gb28181_params->video_adjust_time);
	set_property(video_adjust_key,buf);
	memset(buf,0,32);
	//视频流类型
	sprintf(buf, "%d", gb28181_params->video_stream_type);
	set_property(video_stream_type_key,buf);
	memset(buf,0,32);
	
	return 1;
}