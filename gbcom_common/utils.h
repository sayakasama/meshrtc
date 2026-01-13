#ifndef  __MESH_UTILS_H__
#define  __MESH_UTILS_H__


#ifdef  __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////
#ifdef MESH_ARM_32
#define MESH_LOG_PRINT(module, __level__, __format__, __arg__...) \
    do{ \
        if  (__debug_printf__) \
        { \
			u_int64_t tt_123_ = sys_time_get(); u_int64_t tt_seconds__ = tt_123_/1000; int tt_ms__ = tt_123_%1000; \
			printf("[%llu.%03d][%s][%s:%d]: " __format__ "\n", tt_seconds__, tt_ms__, module, __FUNCTION__, __LINE__, ##__arg__); \
        }\
		u_int64_t tt_123_ = sys_time_get(); u_int64_t tt_seconds__ = tt_123_/1000; int tt_ms__ = tt_123_%1000; \
		syslog_ext(__level__, "[%llu.%03d][%s][%s:%d]: " __format__ "\n", tt_seconds__, tt_ms__, module, __FUNCTION__, __LINE__, ##__arg__); \
    }while(0)
#else
#define MESH_LOG_PRINT(module, __level__, __format__, __arg__...) \
    do{ \
        if  (__debug_printf__) \
        { \
			u_int64_t tt_123_ = sys_time_get(); u_int64_t tt_seconds__ = tt_123_/1000; int tt_ms__ = tt_123_%1000; \
			printf("[%lu.%03d][%s][%s:%d]: " __format__ "\n", tt_seconds__, tt_ms__, module, __FUNCTION__, __LINE__, ##__arg__); \
        }\
		u_int64_t tt_123_ = sys_time_get(); u_int64_t tt_seconds__ = tt_123_/1000; int tt_ms__ = tt_123_%1000; \
		syslog_ext(__level__, "[%lu.%03d][%s][%s:%d]: " __format__ "\n", tt_seconds__, tt_ms__, module, __FUNCTION__, __LINE__, ##__arg__); \
    }while(0)
#endif

//syslog_ext(__level__, "[%s][%s:%d]:" __format__,  module, __FUNCTION__, __LINE__, ##__arg__);
#define sleep_ms(x) usleep(1000 * x)
//////////////////////////////////////////////////////////////////////////
extern int __debug_printf__;

void     mesh_log_file_set(const char *file);
void     mesh_log_close();
int      syslog_ext(int level, const char *fmt,...);
void     debug_printf_on(int on);
//////////////////////////////////////////////////////////////////////////
u_int64_t sys_time_get(void);
void      sys_date_get(char *date);

int      local_ip_mac_get(const  char *if_name, unsigned char *ip, unsigned char *mac);
int      popen_cmd(const char *szCmd, char *pcBuf, u_int32_t uiBufLength);
int      android_system(const char * cmdstring);
bool     is_ipv4_addr(const char *ip);

void     ip_addr_to_string(unsigned char *addr, char *str);
int      string_to_ip_addr(char *str,  char *addr);
//////////////////////////////////////////////////////////////////////////
int tmp_status_get(const char *file_name, char *status, int max_size);
int tmp_status_set(const char *file_name, const char *status);
//////////////////////////////////////////////////////////////////////////
void singleton_lock_free();
bool singleton_lock_get(const char *module_name);
//////////////////////////////////////////////////////////////////////////
int  module_id_get(const char *module_name);
int  module_name_get(int id, char *module_name);

int  msg_str_get(unsigned int msg_type, char *msg_str);
//////////////////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif