/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <stdbool.h>
#include <switch.h>
#include <switch_stun.h>
#include <switch_curl.h>
#include <quickjs.h>
#include <quickjs-libc.h>
#include <js_xml.h>
#include <js_dtmf.h>
#include <js_odbc.h>
#include <js_coredb.h>
#include <js_codec.h>
#include <js_file.h>
#include <js_socket.h>
#include <js_event.h>
#include <js_coredb.h>
#include <js_filehandle.h>
#include <js_eventhandler.h>
#include <js_session.h>
#include <js_curl.h>

#ifndef true
 #define true SWITCH_TRUE
#endif
#ifndef false
 #define false SWITCH_FALSE
#endif

#define JID_NONE 0x0
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define QJS_IS_NULL(jsV)  (JS_IsNull(jsV) || JS_IsUndefined(jsV) || JS_IsUninitialized(jsV))

#define MOD_VERSION "1.4 (a2)"

typedef struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts_map;
    switch_hash_t           *scripts_map;
    size_t                  cfg_rt_mem_limit;
    size_t                  cfg_rt_stk_size;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    uint32_t                jbseq;
    int                     active_threads;
} globals_t;

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_interrupt;
    switch_size_t           script_len;
    uint32_t                sem;
    char                    *id;
    char                    *name;
    char                    *path;
    char                    *script_buf;
    char                    *args;
    const char              *session_id;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_classes_map;
    switch_hash_t           *classes_map;
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSRuntime               *rt;
    void                    *opaque;
} script_t;

/* utils.c */
uint32_t gen_job_id();
char *safe_pool_strdup(switch_memory_pool_t *pool, const char *str);
uint8_t *safe_pool_bufdup(switch_memory_pool_t *pool, uint8_t *buffer, switch_size_t len);

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data);
void thread_finished();

void js_ctx_dump_error(script_t *script, JSContext *ctx);
switch_status_t js_register_classid(JSRuntime *rt, const char *class_name, JSClassID class_id);
JSClassID js_lookup_classid(JSRuntime *rt, const char *class_name);

switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool);
uint32_t script_sem_take(script_t *script);
void script_sem_release(script_t *script);
script_t *script_lookup(char *id);


#endif
