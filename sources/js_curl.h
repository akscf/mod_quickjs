/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_CURL_H
#define JS_CURL_H
#include "mod_quickjs.h"
#include "curl_hlp.h"

#define CURL_QUEUE_SIZE 128

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
    uint32_t                job_seq;
    uint32_t                active_jobs;
    uint8_t                 fl_destroying;
} js_curl_t;

typedef struct {
    uint32_t    jid;
    uint32_t    http_code;
    uint32_t    body_len;
    uint8_t     *body;
} js_curl_result_t;

typedef struct {
    uint32_t                jid;
    switch_memory_pool_t    *pool;
    curl_conf_t             *curl_conf;
    js_curl_t               *js_curl_ref;
} js_curl_creq_conf_t;

/* js_curl.c */
JSClassID js_curl_get_classid(JSContext *ctx);
switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj);

/* js_curl_misc.c */
switch_status_t js_curl_result_alloc(js_curl_result_t **result, uint32_t http_code, uint8_t *body, uint32_t body_len);
void js_curl_result_free(js_curl_result_t **result);

switch_status_t js_curl_creq_conf_alloc(js_curl_creq_conf_t **conf);
void js_curl_creq_conf_free(js_curl_creq_conf_t **conf);

switch_status_t js_curl_job_can_start(js_curl_t *js_curl);
void js_curl_job_finished(js_curl_t *js_curl);

void js_curl_queue_clean(js_curl_t *js_curl);
uint32_t js_curl_job_next_id(js_curl_t *js_curl);

#endif

