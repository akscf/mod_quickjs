/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_CURL_H
#define JS_CURL_H
#include "mod_quickjs.h"

typedef struct {
    long                    auth_type;
    uint32_t                connect_timeout;
    uint32_t                request_timeout;
    uint32_t                method;
    uint8_t                 fl_ssl_verfypeer;
    uint8_t                 fl_ssl_verfyhost;
    char                    *url;
    char                    *cacert;
    char                    *user_agent;
    char                    *credentials;
    char                    *content_type;
    char                    *proxy_credentials;
    char                    *proxy_cacert;
    char                    *proxy;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_queue_t          *events;
    uint32_t                jobs;
} js_curl_t;

typedef struct {
    uint32_t    jid;
    uint32_t    http_code;
    uint32_t    body_len;
    uint8_t     *body;
} js_curl_result_t;

JSClassID js_curl_get_classid(JSContext *ctx);
switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj);

#endif

