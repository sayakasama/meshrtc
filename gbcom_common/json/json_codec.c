#include "mesh_rtc_inc.h"

#include "cjson.h"
#include "json_codec.h"

///////////////////////////////////////////////////////////////////
typedef struct tag_cjson_handler {
	char key[JSON_KEY_MAX_LEN];
	int (*fn)(void *p, const char *value_str, int value_int);
}cjson_handler_t;

cjson_handler_t cjson_handler_list[JSON_HANDLER_MAX_NUM] = {0};
int cjson_handler_index = 0;
///////////////////////////////////////////////////////////////////
int register_cjson_handler(const char *key, int (*fn)(void *p, const char *value_str, int value_int))
{
	cjson_handler_t *handler = &cjson_handler_list[cjson_handler_index]; 
	
	if(cjson_handler_index >= JSON_HANDLER_MAX_NUM)
	{
		return MESH_RTC_FAIL;
	}
	
	strncpy(handler->key, key, JSON_KEY_MAX_LEN - 1);
	handler->fn = fn;
	
	cjson_handler_index++;
	
	return MESH_RTC_OK;
}
///////////////////////////////////////////////////////////////////
int json_msg_decode(char *msg, void *p)
{
	int loop, ret;
	cjson_handler_t *handler;
    cJSON * pData = cJSON_Parse(msg);
	cJSON * value;
	
    if(pData == NULL)
	{               
        MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, " cJSON_Parse recv data failed !");
        return MESH_RTC_FAIL;    
    }

	// find all key and try to decode msg item
	for(loop=0; loop < cjson_handler_index; loop++)
	{
		handler = &cjson_handler_list[loop];
		value = cJSON_GetObjectItem(pData, handler->key);
		if(value)
		{
			ret = handler->fn(p, value->valuestring, value->valueint);
			if(MESH_RTC_FAIL == ret)
			{
				MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "json msg decode error:  key=%s value=%s\n", handler->key, value->valuestring);
				break;
			}
		} 
    }
       
    cJSON_Delete(pData); 
    return MESH_RTC_OK;	
}
///////////////////////////////////////////////////////////////////
void *json_msg_encode_start(void)
{
    cJSON* pJson = cJSON_CreateObject();
	return pJson;
}
///////////////////////////////////////////////////////////////////
void json_msg_add_int(void *p, const char *key, int value)
{
    cJSON_AddItemToObject((cJSON* )p, key, cJSON_CreateNumber(value));	
}

///////////////////////////////////////////////////////////////////
void json_msg_add_string(void *p, const char *key, const char *value)
{
    cJSON_AddItemToObject((cJSON* )p, key, cJSON_CreateString(value));	
}
///////////////////////////////////////////////////////////////////
void json_msg_encode_end(void *p, char *buf, int buf_size)
{
	cJSON* pJson = (cJSON* )p;
	char*  content;
    
	if(!p || !buf)
	{
		return;
	}
	
	buf[0] = 0;
	
	content = cJSON_PrintUnformatted(pJson);
	strncpy(buf, content, buf_size);
	
    MESH_LOG_PRINT(COMMON_MODULE_NAME, LOG_INFO, "json msg:  %s\n", content);
	
    free(content);
	
    cJSON_Delete(pJson);
	
	return;
}
