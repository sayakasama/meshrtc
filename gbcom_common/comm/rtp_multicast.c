#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "udp_comm.h"
#include "multicast.h"
#include "rtp_session.h"
#include "rtp_multicast.h"

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
typedef struct tag_rtp_multicast {
	int send_sn;

	int send_sock;
	int recv_sock;
	char group_ip[IP_STR_MAX_LEN];
	char interface[64]; // if name such as: lan123456
	int  group_port;
}rtp_multicast_t;

rtp_multicast_t  rtp_multicast;
////////////////////////////////////////////////////////////////////////////////////////////
void rtp_multicast_release(void)
{
	rtp_multicast_t *multicast = (rtp_multicast_t *)&rtp_multicast;
	
	if(multicast->send_sock > 0)
	{
		close(multicast->send_sock);
		multicast->send_sock = -1;
	}
	
	if(multicast->recv_sock > 0)
	{
		close(multicast->recv_sock);
		multicast->recv_sock = -1;
	}
}
////////////////////////////////////////////////////////////////////////////////////////////
int rtp_multicast_init(const char *group_ip, int group_port, const char *interface)
{
	rtp_multicast_t *multicast = (rtp_multicast_t *)&rtp_multicast;

	memset(multicast, 0, sizeof(rtp_multicast_t));

	// save multicast addr info
	strncpy(multicast->interface, interface, IP_STR_MAX_LEN);
	strncpy(multicast->group_ip, group_ip, IP_STR_MAX_LEN);
	multicast->group_port = group_port;
	
	// create multicast sock
	multicast->recv_sock = multicast_recv_sock_create(group_ip, group_port, interface);
	
	multicast->send_sock = multicast_send_sock_create(interface);
	unsigned char loop = 0; // prohibit local loop
	setsockopt(multicast->send_sock, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));
	
	if((multicast->send_sock > 0) && (multicast->recv_sock > 0))
	{
		return MESH_RTC_OK;
	} else {
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "create multi cast sock failed ");
		return MESH_RTC_FAIL;	
	}
}
////////////////////////////////////////////////////////////////////////////////////////////
int rtp_multicast_send(const char *data, int len, int data_type)
{
	int ret, pkt_len, send_bytes;
	char local_buf[RTP_PAKCET_MAX_SIZE];
	rtp_head_t  *rtp_head = (rtp_head_t *)local_buf;
	pkt_len    = 1400;
	rtp_multicast_t *multicast = (rtp_multicast_t *)&rtp_multicast;

	if(multicast->send_sock < 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "multicast  sesson is not valid  ");				
		return 0;
	}
	
	if(len > pkt_len)
	{
		return 0; // not support fragment in multicast
	}
		
	memset(rtp_head, 0, sizeof(rtp_head_t));	
	rtp_head->v  = MESH_RTP_VERSION;	
	rtp_head->pt = data_type;	
	rtp_head->sn = multicast->send_sn++;
	rtp_head->timestamp = 0; // todo. ms * 90000
	
	memcpy(local_buf + sizeof(rtp_head_t), data, len);
	send_bytes = len + sizeof(rtp_head_t);

	ret = multicast_send_data(multicast->send_sock, local_buf, send_bytes, multicast->group_ip, multicast->group_port);
	
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////
int  rtp_multicast_recv(char *data, int *data_len, int *data_type, int max_size, int *sn, int *timestamp)
{
	rtp_multicast_t *multicast = (rtp_multicast_t *)&rtp_multicast;
	rtp_head_t     *rtp_head;	
	int len;
	char buf[RTP_PAKCET_MAX_SIZE];
	
	if(multicast->recv_sock < 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "multicast sesson is not valid  ");				
		return 0;
	}

	len = multicast_recv_data(multicast->recv_sock, buf, RTP_PAKCET_MAX_SIZE);	
	if(0 == len)
	{
		return 0;
	}
	
	if(len > max_size)
	{
		return 0;
	}
		
	rtp_head = (rtp_head_t *)buf;
	if(MESH_RTP_VERSION != rtp_head->v)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "multicast version is not %d !!! rtp_head->v=%d ", MESH_RTP_VERSION, rtp_head->v);		
		return 0;
	}

	*data_len  = len - sizeof(rtp_head_t); 	
	memcpy(data, buf + sizeof(rtp_head_t), *data_len);
	*sn        = rtp_head->sn;
	*data_type = rtp_head->pt;
	*timestamp = rtp_head->timestamp;
	
	return 1;	
}