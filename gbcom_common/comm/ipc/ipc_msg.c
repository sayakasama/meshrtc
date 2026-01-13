#include "mesh_rtc_inc.h"
#include "mesh_rtc_common.h"

#include "ipc_msg.h"

const char *ipc_root_path = "/tmp";
extern unsigned int local_module_id;
////////////////////////////////////////////////////////////////////////////////////////
int ipc_msg_init(const char *module_name, int queue_size)
{
    key_t key;
	char buf[512];
	struct msqid_ds msg_qbuf;
	FILE *f;
	
	sprintf(buf, "%s/%s", ipc_root_path, module_name);
	f = fopen(buf, "w+");
	if(f)
	{
		fclose(f);
	}
	
    key = ftok(buf, 'm');
	
	//TODO. If ipc queue exist, delete it firstly.?
    int msgid = msgget(key, IPC_CREAT | 0777);
    if (msgid < 0)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msgid init error! module_name=%s ", module_name);
        return -1;
    }
	
	if(queue_size < IPC_MSG_QUEUE_MIN_SIZE)
	{
		queue_size = IPC_MSG_QUEUE_MIN_SIZE; 
	}
	
	if( msgctl(msgid, IPC_STAT, &msg_qbuf) == -1) 
	{
		return msgid;	
	}
	//msg_qbuf.msg_perm.uid = getuid();  
	msg_qbuf.msg_qbytes   = queue_size;
	if (msgctl(msgid, IPC_SET, &msg_qbuf) == -1) 
	{
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msgctl IPC_SET size %d %s error=%d", queue_size, strerror(errno), errno);
		return msgid;
	}
	
	return msgid;
}

////////////////////////////////////////////////////////////////////////////////////////
int ipc_msg_recv_inner(int msgid, char *msg, int *msg_size, int max_size)
{
    int  size;
	int  msg_type  = 0; // get first msg from queue。 IPC_NOWAIT
	int  wait_type = 0; // blocked for msg 
    msg_buf_t msg_buf;

    if (msgid < 0)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msgid error! %d ", msgid);
        return MESH_RTC_FAIL;
    }
	
    memset(&msg_buf, 0, sizeof(msg_buf_t));
	while(1)
	{
		sleep_ms(2);
		
		size = msgrcv(msgid, &msg_buf, IPC_MSG_MAX_SIZE, msg_type, wait_type);
		if (size > 0)
		{
			// get a msg from queue
			if(size > max_size)
			{
				MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msg buf is too small %d < real msg size %d ", max_size, (int)size);
				return MESH_RTC_FAIL;			
			}		
			
			memcpy(msg, msg_buf.msg_content, size);
			*msg_size = size;
			//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "recv data:%s\n", msg_buf.msg_content);
			return MESH_RTC_OK;			
		}

		if(EACCES == errno)
		{
			MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msg rcv error! %s errno=%d ", strerror(errno), errno);
			return MESH_RTC_FAIL;			
		}
		
		if((EINTR == errno) || (ENOMSG == errno))
		{
			continue;
		} else {
				/*
				E2BIG：消息数据长度大于msgsz而msgflag没有设置IPC_NOERROR
				EIDRM：标识符为msqid的消息队列已被删除
				EACCESS：无权限读取该消息队列
				EFAULT：参数msgp指向无效的内存地址
				ENOMSG：参数msgflg设为IPC_NOWAIT，而消息队列中无消息可读
				*/
		}
			
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msg rcv error! %s errno=%d ", strerror(errno), errno);
		return MESH_RTC_FAIL;
	}
		
	return MESH_RTC_OK;
}

////////////////////////////////////////////////////////////////////////////////////////
int ipc_msg_send_inner(int msgid, const char *msg, int msg_size)
{
	int  ret, count;
	int  msg_type  = 1; // get first msg from queue。 
	int  wait_type = IPC_NOWAIT; // blocked for msg. if  not wait, use IPC_NOWAIT
    msg_buf_t msg_buf;

    if (msgid < 0)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc_msg_send : msgid error! %d ", msgid);
        return MESH_RTC_FAIL;
    }
	
	if(msg_size > IPC_MSG_MAX_SIZE)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc_msg_send : msg size %d too big > IPC_MSG_MAX_SIZE %d ", msg_size, IPC_MSG_MAX_SIZE);
        return MESH_RTC_FAIL;
    }
	
	count = 0;
    memset(&msg_buf, 0, sizeof(msg_buf_t));
    msg_buf.mtype = msg_type;
	memcpy(msg_buf.msg_content, msg, msg_size);
	while(count++ < 10) // max retry 10 times
	{
		ret = msgsnd(msgid,(void *)&msg_buf, msg_size, wait_type);
		if(ret == 0)
		{
			break; // send msg ok
		}

		if((errno == EAGAIN) || (errno == EINTR))
		{
			sleep_ms(1);
			continue;
		} 

		/*
			EIDRM：标识符为msqid的消息队列已被删除
			EACCESS：无权限写入消息队列
			EFAULT：参数msgp指向无效的内存地址
			EINVAL：无效的参数msqid、msgsz或参数消息类型type小于0
		*/		
		
		MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "msg snd error!");
		return MESH_RTC_FAIL;
	}
    
	return MESH_RTC_OK;
}
////////////////////////////////////////////////////////////////////////////////////////
int ipc_msg_send(const char *module_name, const char *msg, int msg_size)
{
    key_t key;
	int msgid;
	char buf[512];

	sprintf(buf, "%s/%s", ipc_root_path, module_name);
	
	// IPC MSG QUEUE always be created by ipc msg receiver
    key   = ftok(buf, 'm');
    msgid = msgget(key, 0);
    if (msgid < 0)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc msg queue msgid  get error! module_name=%s", module_name);
        return -1;
    } else {
		//MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc_msg_send module_name=%s msg=%s", module_name, msg);
	}
	
	return ipc_msg_send_inner(msgid, msg, msg_size);
}	
////////////////////////////////////////////////////////////////////////////////////////
int   ipc_msg_send_ext(const char *module_name, unsigned int msg_type, unsigned char sn, const char *msg, int msg_size)
{
    key_t key;
	int   msgid;
	char  buf[512];
	char  data[IPC_MSG_MAX_SIZE];
	common_msg_head_t *msg_head = (common_msg_head_t *)data;
	
	sprintf(buf, "%s/%s", ipc_root_path, module_name);
	
	// IPC MSG QUEUE always be created by ipc msg receiver
    key   = ftok(buf, 'm');
    msgid = msgget(key, 0);
    if (msgid < 0)
    {
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_ERR, "ipc msg queue msgid  get error! module_name=%s", module_name);
        return -1;
    }
	
	msg_head->src_module_id = local_module_id;
	msg_head->dst_module_id = module_id_get(module_name);
	msg_head->msg_type   = msg_type;
	msg_head->msg_len    = msg_size;
	msg_head->msg_sn     = sn; // default is 0, increase one if same msg resend
	msg_head->rsv        = 0;
	memcpy(msg_head->msg_data, msg, msg_size);
	
	return ipc_msg_send_inner(msgid, (char *)msg_head, msg_size + sizeof(common_msg_head_t));
}	
