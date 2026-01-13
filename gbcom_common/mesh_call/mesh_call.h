#ifndef  __MESH_CALL_H__
#define  __MESH_CALL_H__

#ifdef  __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////
typedef enum enum_mesh_call_status {
	MESH_CALL_IDLE,
	MESH_CALL_START,
	MESH_CALL_LOGINED,
	MESH_CALL_CONNECTING,
	MESH_CALL_CONNECTED,
	MESH_CALL_DISCONN
} mesh_call_status_e;

typedef enum enum_mesh_call_event {
	MESH_EVENT_INIT,
	MESH_EVENT_TIMEOUT,
	
	MESH_EVENT_CALL_START,  // local start call
	MESH_EVENT_CALL_HUNGUP, // local stop call
	MESH_EVENT_CALL_STOP,   // peer stop call
	MESH_EVENT_CALL_IND,    // peer call incoming
	MESH_EVENT_DISCONN_IND, // disconn ind
	
	MESH_EVENT_PEER_BUSY,
	MESH_EVENT_CALL_FAIL,
	
	MESH_EVENT_LOGIN_IND,
	MESH_EVENT_LOGOUT_IND,
	MESH_EVENT_LOGIN_FAIL,
	
	MESH_EVENT_RTC_SERVER_ONLINE,
	MESH_EVENT_RTC_SERVER_OFFLINE
} mesh_call_event_e;
////////////////////////////////////////////////////////////////////////////////////////
// mesh ui call following function to setup a cmd
int  mesh_call_ui_cmd_set(const char *cmd);
// mesh ui call following function to get current status/ind
int  mesh_call_ui_ind_get(char *ind, int max_size);

// mesh ui create a thread to handle mesh call
void *mesh_call_thread(void *p);

// get latest photo file
int  mesh_photo_get(char *file_name);
// mesh ui create a thead to get photo
void *mesh_photo_thread(void *p);

int  mesh_call_get_state();
////////////////////////////////////////////////////////////////////////////////////////

#ifdef  __cplusplus
}
#endif


#endif // __MESH_CALL_H__
