#include "mesh_rtc_inc.h"

#include "multicast.h"

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
int ifaddr_get(const char *interface, char* addr)
{
    int sock = socket (AF_INET, SOCK_DGRAM, 0);

    struct ifreq ifr;
    memset (&ifr, 0, sizeof (ifr));
    strcpy (ifr.ifr_name, interface);
    if (ioctl (sock, SIOCGIFADDR, &ifr) < 0) 
    {
        close (sock);
        return MESH_RTC_FAIL;
    }
  
    strcpy (addr, inet_ntoa(((struct sockaddr_in* ) &(ifr.ifr_addr))->sin_addr));
    close (sock);

    return MESH_RTC_OK;
}

//////////////////////////////////////////////////////////////////////////////
int is_multicast_addr(const char *group_ip)
{
	int net = 0;
	const char *p;
	
	p = group_ip;
	while((*p != 0) && (*p != '.'))
	{
		net = net * 10 + (*p - '0');
		p++;
	}
	
	// 239.*.*.* <=> 224.*.*.* is ip multicast addr	
    if ((net < 224) || (net > 239)) 
	{
	    return 0;
	}
	
	return 1;
}	
//////////////////////////////////////////////////////////////////////////////
int multicast_recv_sock_create(const char *group_ip, int group_port, const char *interface)
{
    int    sockfd;
    struct sockaddr_in local_recv_addr;
	int    so_reuseaddr = 1;
    struct ip_mreq mreq;

    // join multicast group 
	
    if (!is_multicast_addr(group_ip)) 
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "multicast_server_sock_create failed \n");
	    return MESH_RTC_FAIL;
	}
	
	// local server init
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to socket for multicast \n");
        return MESH_RTC_FAIL;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) 
	{  
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to setup reuse udp socket for multicast \n");
    }  
    
    local_recv_addr.sin_family      = AF_INET;
	local_recv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");	// INADDR_ANY	
    local_recv_addr.sin_port        = htons(group_port);

    if(bind(sockfd, (struct sockaddr*)&local_recv_addr, sizeof(local_recv_addr)) == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to bind udp socket: %s errno=%d", strerror(errno), errno);
		close(sockfd);
        return MESH_RTC_FAIL;
    }

	memset(&mreq, 0, sizeof(mreq));
	unsigned char ip[32]  = {0};
	
#ifdef LINUX_X86
    mreq.imr_multiaddr.s_addr = inet_addr(group_ip); 
	// any port recv multicast
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);                	
#else	
	unsigned char mac[32] = {0};	

	local_ip_mac_get(interface, ip, mac);	
    mreq.imr_multiaddr.s_addr = inet_addr(group_ip);
	// any port recv multicast
    mreq.imr_interface.s_addr = inet_addr((char *)ip);                	
#endif

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) 
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "join multicast group failed? %s error=%d ip=%s group_ip=%s", strerror(errno), errno, ip, group_ip);
    }
	
	return sockfd;
}

//////////////////////////////////////////////////////////////////////////////	
int multicast_send_sock_create(const char *interface)
{
    int sockfd;
	char local_ip[32] = "\0";
    struct in_addr outputif;

	// local server init
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to socket");
        return MESH_RTC_FAIL;
    }

    ifaddr_get(interface, local_ip);
    outputif.s_addr = inet_addr (local_ip);
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char* ) &outputif, sizeof(struct in_addr)))
    {

    }
	
	return sockfd;
}
	
//////////////////////////////////////////////////////////////////////////////		
int  multicast_send_data(int sockfd, char *data, int data_len, const char *group_ip, int group_port)
{
    socklen_t len = sizeof(struct sockaddr_in);
	int ret;
	struct sockaddr_in group_addr;

    if (!is_multicast_addr(group_ip)) 
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "group_ip is not valid %s \n", group_ip);
	    return 0;
	}

    memset(&group_addr, 0, sizeof(struct  sockaddr_in));
    group_addr.sin_family      = AF_INET;
    group_addr.sin_addr.s_addr = inet_addr(group_ip);
    group_addr.sin_port        = htons(group_port);
	
    ret = sendto(sockfd, data, data_len, 0, (struct sockaddr *)&group_addr, len);
    if (ret == -1)
    {
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "multicast send failed: %s errno=%d \n", strerror(errno), errno);		
		if(errno == ENODEV)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "multicast exit and restart to fix this problem");		
			// In some case  ip or net dev changed, we must restart soft to get around the problem
			exit(0);
		}
		return -1;
    }
	
    return ret;
}

//////////////////////////////////////////////////////////////////////////////		
int multicast_recv_data(int sockfd, char *buf, int buf_size)
{
//    socklen_t len = sizeof(struct sockaddr);
//	struct sockaddr_in client_addr;
	int ret;
	
    ret = recvfrom(sockfd, buf, buf_size, MSG_DONTWAIT, NULL, NULL);  
    if (ret < 0) 
	{  
		if(EAGAIN == errno)
		{
			return 0; // no data
		}
		//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "recvfrom error:  %d %s", errno, strerror(errno));				
		return -1;
    }  
	
    return ret;
}
////////////////////////////////////////////////////////////////////////////////
/*
# 2. 看系统有没有过滤组播包：
# 2.1 看接受组播的网卡是否过滤了：
cat /proc/sys/net/ipv4/conf/en4/rp_filter
# 如果是0， good。

# 2.2 看all网卡是否过滤了：
cat /proc/sys/net/ipv4/conf/all/rp_filter
# 如果是0， good。

# 这两个值都必须是0，才行！如果不是0，这样修改：
# 2.3 临时修改取消过滤：
sudo sysctl -w net.ipv4.conf.en4.rp_filter=0
sudo sysctl -w net.ipv4.conf.all.rp_filter=0
# 2.4 永久修改取消过滤（重启亦有效）：
sudo vi /etc/sysctl.conf
# 改为：
net.ipv4.conf.default.rp_filter=0
net.ipv4.conf.all.rp_filter=0
*/	