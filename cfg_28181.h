#ifndef  __CFG_28181_H__
#define  __CFG_28181_H__

#ifdef __cplusplus
extern "C"{
#endif

typedef struct tag_28181_cfg {
	unsigned int  gb28181_enable;   //模块是否启用
	unsigned char server_type[32];  //服务器类型
	unsigned char peer_realm[16];   //SIP 服务器域
	unsigned char peer_sip_id[32];  //SIP 服务器ID
	unsigned char peer_ip[32];      //SIP 服务器地址
	unsigned int  peer_port;        //SIP 服务器端口

	unsigned char local_realm[16];  //SIP用户域
	unsigned char local_sip_id[32]; //SIP用户认证ID 
	unsigned int  local_port;       //SIP用户端口
	unsigned char local_av_channel[32]; //本地视频通道ID

	unsigned char user_name[32];    //SIP用户名
	unsigned char passwd[32];		//密码

	unsigned int  is_tcp;           //信令传输协议(1 tcp;0 udp) 

	unsigned int  expire_time;      //注册有效期(秒)
	unsigned int  heartbeat_period; //心跳周期(秒)
	unsigned int  register_period;  //注册间隔(秒)
	unsigned int  max_heartbeat_overtime; //最大心跳超时次数
	unsigned int  video_adjust_time;    //视频分辨率自适应调整周期

	unsigned int  video_stream_type; //视频流类型 0：视频音频双流 1：单视频流 2：音视频复合流
} __attribute__((packed)) gb_28181_cfg_t;

typedef struct gb28181_client_cfg
{
	unsigned int refresh_reg;
	unsigned int rtcp_recv;
	unsigned int rtcp_send;
	unsigned int audio_recv_tcp;
	unsigned int audio_recv_ps;
}__attribute__((packed)) gb_client_conf_t;


///////////////////////////////////////////////////////////////////
int gb_28181_cfg_get(gb_28181_cfg_t *gb28181_params);
int gb_28181_cfg_set(gb_28181_cfg_t *gb28181_params);

#ifdef __cplusplus
}
#endif

#endif