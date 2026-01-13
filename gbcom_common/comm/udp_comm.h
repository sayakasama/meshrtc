#ifndef __UDP_COMM_H__
#define __UDP_COMM_H__

#ifdef  __cplusplus
extern "C" {
#endif

int  udp_sock_create(const char *peer_ip_addr, int peer_port, const char *local_ip_addr, int local_port, struct sockaddr_in *peer_udp_addr);

int  udp_recv(int sockfd, char *buf, int buf_size, struct sockaddr_in *peer_udp_addr);
int  udp_send(int sockfd, char *buf, int buf_size, struct sockaddr_in *peer_udp_addr);
int  udp_close(int sockfd);


#ifdef  __cplusplus
}
#endif


#endif