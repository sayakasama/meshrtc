#ifndef __MESH_RTC_INC_H__
#define __MESH_RTC_INC_H__

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <float.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <sys/wait.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <netinet/in.h>
#ifdef   NO_ANDROID
#include <curl/curl.h>
#endif
#include <arpa/inet.h>

#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdbool.h>

#include "utils.h"
#include "mesh_rtc_common.h"

#if  !defined(NO_ANDROID) && !defined(LINUX_X86)
extern int msgget(key_t key, int msgflg);
extern int msgctl(int msqid, int cmd, struct msqid_ds *buf);
extern ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,int msgflg);
extern int msgsnd(int msqid, void *msgp, size_t msgsz, int msgflg);
#endif

#endif