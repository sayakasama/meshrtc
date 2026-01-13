#ifndef __MESH_MULTICAST_H__
#define __MESH_MULTICAST_H__

#ifdef  __cplusplus
extern "C" {
#endif


int multicast_recv_sock_create(const char *group_ip, int group_port, const char *interface);
int multicast_recv_data(int sockfd, char *buf, int buf_size);

int multicast_send_sock_create(const char *interface);
int multicast_send_data(int sockfd, char *data, int data_len, const char *group_ip, int group_port);


#ifdef  __cplusplus
}
#endif


#endif