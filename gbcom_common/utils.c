#include "mesh_rtc_inc.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>   
#include <stdarg.h>
	   
#ifndef ETH_ALEN
#define ETH_ALEN            6
#endif
////////////////////////////////////////////////////////////////////////////////////
const char *mac_format = "%02x:%02x:%02x:%02x:%02x:%02x";
const char *ip_format  = "%d.%d.%d.%d";
///////////////////////////////////////////////////////////////
static int    singleton_fd     = -1;
unsigned int  local_module_id  = -1;
static 	char  lock_file[256];
char  module_log1_file_name[256];
char  module_log2_file_name[256];

FILE       *log_fp        = NULL;
const char *log1_file_name = "/data/local/log/rtc.log";
const char *log2_file_name = "/data/local/log/rtc.log.1";
//////////////////////////////////////////////////////////////////////////
#ifdef MESH_RELEASE_VERSION // define this macro in top makefile or cmakefile
int  __debug_printf__ = 0;
#else
int  __debug_printf__ = 1;
#endif

void   debug_printf_on(int on)
{
	__debug_printf__ = on;
}
////////////////////////////////////////////////////////////////////////////////////
u_int64_t sys_time_get(void)
{
	struct timeval tv;
	u_int64_t ms;
	
	gettimeofday(&tv, NULL);
	ms  = tv.tv_sec * 1000 + tv.tv_usec / 1000;
	return ms;
}	
////////////////////////////////////////////////////////////////////////////////////
void mac_addr_to_string(unsigned char *addr, char *str)
{
	sprintf(str, mac_format, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}
////////////////////////////////////////////////////////////////////////////////////
int string_to_mac_addr(char *str,  char *addr)
{
	int ret;
	
	ret = sscanf(str, mac_format, &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
	if(ret != 6)
	{
		return -1;
	}
	
	return 0;
}
////////////////////////////////////////////////////////////////////////////////////
void ip_addr_to_string(unsigned char *addr, char *str)
{
	sprintf(str, ip_format, addr[0], addr[1], addr[2], addr[3]);
}
////////////////////////////////////////////////////////////////////////////////////
int string_to_ip_addr(char *str,  char *addr)
{
	int ret;
	int a1,a2,a3,a4;
	
	ret = sscanf(str, ip_format, &a1, &a2, &a3, &a4);
	if(ret != 4)
	{
		return -1;
	}
	
	addr[0] = a1;
	addr[1] = a2;
	addr[2] = a3;
	addr[3] = a4;
	
	return 0;
}

////////////////////////////////////////////////////////////////////////////
 bool is_ipv4_addr(const char *ip)
 {
     int dots_num = 0; 
     int section_sum = 0; 
	 int count = 0; 
 
     if ((NULL == ip) || (ip[0] == '0') || (ip[0] == '.'))
     { 
         return false;
     }   
 
     while (*ip) 
	 {
         if (*ip == '.') 
		 {
             dots_num++; 
             if ((section_sum <= 255) && (count > 0))
			 { 
                 section_sum = 0;
				 count = 0;
                 ip++;
                 continue;
             } else {  
				return false;
			 }
         }   
         else if (*ip >= '0' && *ip <= '9') 
		 { 
             section_sum = section_sum * 10 + (*ip - '0'); 
			 count++;
         } else {
             return false;
		 }
         ip++;   
     }   

     if ((section_sum <= 255) && (count > 0))
	 {
         if (dots_num == 3) 
		 {
             return true;
         }   
     }   
 
     return false;
}
////////////////////////////////////////////////////////////////////////////////////
void mac_addr_get(const char *if_name, unsigned char *buf)
{
   	int fd;
	struct ifreq ifr;
	char *mac;
	
	memset(buf, 0, ETH_ALEN);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0)
	{
		return;
	}
	
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy((char *)ifr.ifr_name , if_name , IFNAMSIZ-1);

	ioctl(fd, SIOCGIFHWADDR, &ifr);
	close(fd);
	
	mac = (char *)ifr.ifr_hwaddr.sa_data;
    memcpy(buf, mac, ETH_ALEN);	
}
////////////////////////////////////////////////////////////////////////////////////
int  local_ip_mac_get(const  char *if_name, unsigned char *ip_str, unsigned char *mac)
{
	struct ifaddrs* if_addr_list = NULL, *ifa = NULL, *ifc;
	struct in_addr * tmp = NULL;
	unsigned char ip[INET_ADDRSTRLEN];
	char mac_buf[64];
	int  ret = -1;
	unsigned int ip_int;
	
	if(0 != getifaddrs(&if_addr_list))
	{
		return -1;
	}

	ifc = if_addr_list;
	for (ifa = if_addr_list; ifa != NULL; ifa = ifa->ifa_next) 
	{
		if (!ifa->ifa_addr) 
		{ 
			continue; // 
		}
		
		if (ifa->ifa_addr->sa_family != AF_INET) 
		{ 
			continue; // not IPV4 addr
		}
		
		if(strcmp(ifa->ifa_name, if_name))
		{ 
			continue; // not if name
		}
		
		tmp    = &((struct sockaddr_in *) ifa->ifa_addr)->sin_addr;
		ip_int = (int)tmp->s_addr;
		ip_int = ntohl(ip_int);
		ip[0]  = (ip_int>>24) & 0xff;
		ip[1]  = (ip_int>>16) & 0xff;
		ip[2]  = (ip_int>>8) & 0xff;
		ip[3]  = ip_int & 0xff;		
		ip_addr_to_string(ip, (char *)ip_str);
		
		mac_addr_get(if_name, mac);			
		mac_addr_to_string(mac, mac_buf);
		
		//LOG_PRINT(LOG_INFO, "%s IP: %s mac: %s\n", ifa->ifa_name, ip_buf, mac_buf);		
		ret = 1;

		break;
	}
	
	if (ifc) 
	{
        freeifaddrs(ifc);
    }

	return ret;
}	
/////////////////////////////////////////////////////////////////////////////////////////////
int popen_cmd(const char *szCmd, char *pcBuf, u_int32_t uiBufLength)
{
	FILE *pf = NULL;
	char *p;
	int len = 0;
	int ret = 0,wifRet = 0;
	static int errCount = 0;
	
	//pthread_mutex_lock(&g_pthmutex); 

	pf = popen(szCmd, "r");
 	if(pf == NULL)
	{
 		printf( "popen(%s) failed, error:%s(%d).\n", szCmd, strerror(errno), errno);
		//pthread_mutex_unlock(&g_pthmutex);
		return -1;
	}

	if(pcBuf){
		memset(pcBuf,0,uiBufLength);
		p   = fgets(pcBuf, uiBufLength, pf);
		len = strlen(pcBuf);
		if(p && (len > 0) && pcBuf[len - 1] == '\n')
		{
			pcBuf[len -1] = '\0';
		}
	}

	ret = pclose(pf);

	if(ret != 0)
	{
		wifRet = WIFEXITED(ret);
		if(wifRet == 0)
		{
			//异常结束
			int wifs = WIFSIGNALED(ret);
			int wte = 0;
			if(wifs)
			{
				wte = WTERMSIG(ret);//取使子进程结束的信号编号。
			}
			printf("popen(cmd:%s)",szCmd);
			printf("popen(ret:%d,wifRet:%d,Abnormal exit,wifs:%d,wte:%d)", ret,wifRet,wifs,wte);
			
			printf("popen(errCount:%d)",errCount);
			
			errCount ++;
			if(errCount >= 3)
			{
				printf("popen(errCount:%d), exit.",errCount);
				exit(1);
			}
		}
		else
		{
			errCount = 0;
		}
	}
	else
	{
		errCount = 0;
	}
	
	//pthread_mutex_unlock(&g_pthmutex);
	return 0;
}
///////////////////////////////////////////////////////////////////
int android_system(const char * cmdstring)
{
    pid_t pid;
    int status;
 	 
 	if(cmdstring == NULL)
 	{
 	    return (1); 
 	}
		
 	if((pid = fork())<0)
 	{
 	    status = -1; 
 	}
 	else if(pid == 0)
 	{
		// child process
 	    execl("/system/bin/sh", "sh", "-c", cmdstring, (char *)0);
 	    _exit(127); 
 	}
 	else 
 	{
		// parent process
 	    while(waitpid(pid, &status, 0) < 0)
 	    {
 	        if(errno != EINTR)
 	        {
 	            status = -1; //
 	            break;
 	        }
 	    }
 	}
 	 
    return status; 
}
//////////////////////////////////////////////////////////////////////////////////
int tmp_status_set(const char *file_name, const char *status)
{
	FILE *fp;

	fp = fopen(file_name, "w+");
	if(!fp)
	{
		return -1;
	}
	
	fwrite(status, 1, strlen(status), fp);
	fclose(fp);
		
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////
int tmp_status_get(const char *file_name, char *status, int max_size)
{
	FILE *fp;
	int ret;
	
	if(!file_name || !status)
	{
		return -1;
	}
	
	fp = fopen(file_name, "r");
	if(!fp)
	{
		return -1;
	}

	memset(status, 0, max_size);
	ret = fread(status, 1, max_size, fp);
	fclose(fp);		
	if(ret <= 0)
	{
		return -1;
	}
	
	return 1;
}
///////////////////////////////////////////////////////////////
void singleton_lock_free()
{
	int ret;
	
	if(singleton_fd > 0)
	{
#if defined(__ANDROID__)
		if(flock(singleton_fd, LOCK_UN) != 0)
		{
			char module_name[64];
			module_name_get(local_module_id, module_name);
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ULOCK failed: local_module_id=%d module_name=%s", local_module_id, module_name);
		}
		close(singleton_fd);
#else		
		ret = lockf(singleton_fd, F_ULOCK, 0);
		if(ret != 0)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "fail to lockf file %s. errno=%d %s", lock_file, errno, strerror(errno));
		}
#endif		
		close(singleton_fd);
		singleton_fd = -1;
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "free lock file %s", lock_file);
	}
}
///////////////////////////////////////////////////////////////
bool singleton_lock_get(const char *module_name)
{
	int fd = 0;
	
	if(singleton_fd > 0)
	{
		return true;
	}
	
	sprintf(lock_file, "/tmp/%s.lock", module_name);

#if defined(__ANDROID__)

	fd = open(lock_file, O_WRONLY|O_CREAT, 0666);
	if(fd < 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR,  "open file(%s) failed, error:%s(%d).\n", lock_file, strerror(errno), errno);
		return false;
	}
	flock(fd, LOCK_EX);

#else
	
	fd = open(lock_file, O_WRONLY|O_CREAT, 0666);
	if(fd < 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "open file(%s) failed, error:%s(%d).\n", lock_file, strerror(errno), errno);
		return false;
	}

	if(lockf(fd, F_TLOCK, 0) != 0)
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "%s has been started. error:%s(%d). lock_file=%s\n", module_name, strerror(errno), errno, lock_file);
		close(fd);
		return false;
	}

#endif
		
	singleton_fd = fd;
	
	local_module_id = module_id_get(module_name);

	return true;
}
///////////////////////////////////////////////////////////////
int  module_id_get(const char *module_name)
{
	return 0;
}
///////////////////////////////////////////////////////////////
int  module_name_get(int id, char *module_name)
{
	strcpy(module_name, MAIN_MODULE_NAME);
	return 1;
}
///////////////////////////////////////////////////////////////
int  msg_str_get(unsigned int msg_type, char *msg_str)
{
	return 1;
}
///////////////////////////////////////////////////////////////
void  sys_date_get(char *date) 
{
    time_t timestamp = sys_time_get()/1000; 
    struct tm *local_time;
 
	if(!date)
	{
		return;
	}
	
    local_time = localtime(&timestamp);
 
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d", local_time->tm_year + 1900, local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    return ;
}
///////////////////////////////////////////////////////////////
void     mesh_log_close()
{
	if(log_fp)
	{
		fclose(log_fp);
		log_fp = NULL;
	}	
}
///////////////////////////////////////////////////////////////
void  mesh_log_file_set(const char *file)
{
	sprintf(module_log1_file_name, "%s.1.log", file);
	sprintf(module_log2_file_name, "%s.2.log", file);	
	log1_file_name  = module_log1_file_name;
	log2_file_name = module_log2_file_name;
}
///////////////////////////////////////////////////////////////
void replace_file(const char *file1, const char *file2)
{
	char cmd[512];		
	
	sprintf(cmd, "rm %s", file2);
	android_system(cmd);				

	sprintf(cmd, "rename %s %s", file1, file2);
	android_system(cmd);	
}
///////////////////////////////////////////////////////////////
#define LOG_FILE_SIZE  (8*1024*1024)
int syslog_ext(int level, const char*format,...)
{
	int ret;
	static int lines = 0;
	va_list arg_ptr;
    struct stat st;
	
	if(!log_fp)
	{
#ifndef LINUX_X86		
		// TODO: too simple !!!
		if (stat(log1_file_name, &st) != -1) 
		{
			if(st.st_size >= LOG_FILE_SIZE) // > 8M byte
			{
				printf("//////////////////////////////////////// rtc.log file size=%ld ////////////////////////////////////////\n", st.st_size);
				replace_file(log1_file_name, log2_file_name);
				log_fp = fopen(log1_file_name, "w+"); 
			} else {
				log_fp = fopen(log1_file_name, "a+"); 
			}
		} else {
			log_fp = fopen(log1_file_name, "w+"); // a new log file
		}
#endif
	}
	if(!log_fp)
	{
		return 0;
	}
	
	// mutex lock
	va_start(arg_ptr, format);
	ret = vfprintf(log_fp, format, arg_ptr);
	va_end(arg_ptr);
	// mutex unlock

	if(lines++ % 50 != 0)
	{
		return ret;
	}

	if (stat(log1_file_name, &st) == -1) 
	{
		return -1;
	}

	if(st.st_size < LOG_FILE_SIZE) // < 8M byte
	{
		return 0;
	}

	fclose(log_fp);
	log_fp = NULL;
	printf("/////////////////////////// rtc_log file size=%ld > max size %d /////////////////////////\n", st.st_size, LOG_FILE_SIZE);
	replace_file(log1_file_name, log2_file_name);

	return ret;
}