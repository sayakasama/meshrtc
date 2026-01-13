#include "mesh_rtc_inc.h"

#include "udp_comm.h"

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

//struct sockaddr_in peer_udp_addr;
//////////////////////////////////////////////////////////////////////////////
int udp_sock_create(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, struct sockaddr_in *peer_udp_addr)
{
    int sockfd;
    struct sockaddr_in local_recv_addr;
	int so_reuseaddr = 1;

	// local server init
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to socket");
        return MESH_RTC_FAIL;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) 
	{  
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to setup reuse udp socket");
    }  
    
    local_recv_addr.sin_family  = AF_INET;
	if(local_ip_addr)
	{
		local_recv_addr.sin_addr.s_addr = inet_addr(local_ip_addr);		
	} else {
		local_recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	}
    local_recv_addr.sin_port    = htons(local_port);

    if(bind(sockfd, (struct sockaddr*)&local_recv_addr, sizeof(local_recv_addr)) == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to bind udp socket. errno=%d %s", errno, strerror(errno));
		close(sockfd);
        return MESH_RTC_FAIL;
    }

	if(0 != peer_port)
	{
		peer_udp_addr->sin_family = AF_INET; 
		if(peer_ip_addr)
		{
			peer_udp_addr->sin_addr.s_addr = inet_addr(peer_ip_addr);		
		} else {
			peer_udp_addr->sin_addr.s_addr = htonl(INADDR_LOOPBACK); // default addr is 127.0.0.1 for local host comm
		}	
		peer_udp_addr->sin_port = htons(peer_port);
	} else {
		memset(peer_udp_addr, 0, sizeof(*peer_udp_addr));
	}
	
	return sockfd;
}

/////////////////////////////////////////////////////////////////////////////
int  udp_recv(int sockfd, char *buf, int buf_size, struct sockaddr_in *peer_udp_addr)
{
	int len;
    struct sockaddr_in clientaddr;
    socklen_t addrlen = sizeof(struct sockaddr);
     
	// not block on socket  
	len = recvfrom(sockfd, buf, buf_size, MSG_DONTWAIT, (struct sockaddr *)&clientaddr, &addrlen);	
    if(len == -1)
    {
		if(EAGAIN == errno)
		{
			return 0; // no data
		}
		return -1;
    }

	// save udp client addr
	memcpy(peer_udp_addr, &clientaddr, sizeof(clientaddr));
	
    return len;
}

/////////////////////////////////////////////////////////////////////////////
int  udp_send(int sockfd, char *buf, int buf_size, struct sockaddr_in *peer_udp_addr)
{
	int len;
	
	len = sendto(sockfd, buf, buf_size, 0, (struct sockaddr *)peer_udp_addr, sizeof(*peer_udp_addr));
    if(len == -1)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "udp_send failed: sockfd=%d size=%d errno=%d %s", sockfd, buf_size, errno, strerror(errno));
		return 0;
    }	
	
	return len;
}

///////////////////////////////////////////////////////////////////////////////////
int  udp_close(int sockfd)
{
	close(sockfd);
	return 1;
}