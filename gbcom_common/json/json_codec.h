#ifndef __JSON_CODEC_H__
#define __JSON_CODEC_H__

#define  JSON_KEY_MAX_LEN       64
#define  JSON_HANDLER_MAX_NUM   30

#ifdef  __cplusplus
extern "C" {
#endif


int  json_msg_decode(char *msg, void *p);
int  register_cjson_handler(const char *key, int (*fn)(void *p, const char *value_str, int value_int));

void *json_msg_encode_start(void);
void json_msg_add_int(void *p, const char *key, int value);
void json_msg_add_string(void *p, const char *key, const char *value);
void json_msg_encode_end(void *p, char *buf, int buf_size);

#ifdef  __cplusplus
}
#endif


#endif
