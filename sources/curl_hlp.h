/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#ifndef CURL_HLP_H
#define CURL_HLP_H
#include "mod_quickjs.h"

#define CURL_FIELD_TYPE_SIMPLE  0
#define CURL_FIELD_TYPE_FILE    1

#define CURL_METHOD_GET         0
#define CURL_METHOD_PUT         1
#define CURL_METHOD_POST        2
#define CURL_METHOD_DELETE      3

typedef struct {
    int             type;
    char            *name;
    char            *value;
    void            *next;
} xcurl_from_field_t;

typedef struct {
    switch_memory_pool_t    *pool;
    xcurl_from_field_t      *fields;
    xcurl_from_field_t      *fields_tail;
    char                    *url;
    char                    *proxy;
    char                    *proxy_credentials;
    char                    *credentials;
    char                    *content_type;
    char                    *user_agent;
    char                    *cacert;
    char                    *proxy_cacert;
    switch_byte_t           *send_buffer_ref;
    switch_byte_t           *send_buffer;
    switch_buffer_t         *recv_buffer;
    uint32_t                send_buffer_len;
    uint32_t                request_timeout;
    uint32_t                connect_timeout;
    uint32_t                ssl_verfypeer;
    uint32_t                ssl_verfyhost;
    uint32_t                proxy_insecure;
    uint32_t                method;
    long                    auth_type;
    uint32_t                http_error;
    uint8_t                 fl_ext_pool;
} curl_conf_t;

switch_status_t curl_config_alloc(curl_conf_t **curl_config, switch_memory_pool_t *pool, uint8_t with_recvbuff);
void curl_config_free(curl_conf_t *curl_config);

switch_status_t curl_perform(curl_conf_t *curl_config);
switch_status_t curl_field_add(curl_conf_t *curl_config, int type, char *name, char *value);
uint32_t curl_method2id(const char *name);

const char *curl_method2name(uint32_t id);

#endif
