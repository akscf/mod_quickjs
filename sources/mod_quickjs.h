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

#ifndef true
 #define true SWITCH_TRUE
#endif
#ifndef false
 #define false SWITCH_FALSE
#endif

#define QJS_IS_NULL(jsV)  (JS_IsNull(jsV) || JS_IsUndefined(jsV) || JS_IsUninitialized(jsV))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BASE64_ENC_SZ(n) (4*(n/3))
#define BASE64_DEC_SZ(n) ((n*3)/4)
#define JID_NONE    0x0
#define MOD_VERSION "1.6 (a4)"

typedef struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts_map;
    switch_hash_t           *scripts_map;
    size_t                  cfg_rt_mem_limit;
    size_t                  cfg_rt_stk_size;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    int                     active_threads;
} globals_t;

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_interrupt;
    uint8_t                 fl_exit;
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
    switch_mutex_t          *classes_map_mutex;
    switch_hash_t           *classes_map;
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSRuntime               *rt;
    void                    *opaque;
} script_t;

/* utils.c */
char *safe_pool_strdup(switch_memory_pool_t *pool, const char *str);
uint8_t *safe_pool_bufdup(switch_memory_pool_t *pool, uint8_t *buffer, switch_size_t len);

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data);
void thread_finished();

char *audio_file_write(switch_byte_t *buf, uint32_t buf_len, uint32_t samplerate, uint32_t channels, const char *file_ext);

void js_ctx_dump_error(script_t *script, JSContext *ctx);
switch_status_t js_register_classid(JSRuntime *rt, const char *class_name, JSClassID class_id);
JSClassID js_lookup_classid(JSRuntime *rt, const char *class_name);

switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool);
uint32_t script_sem_take(script_t *script);
void script_sem_release(script_t *script);
void script_wait_unlock(script_t *script);
script_t *script_lookup(char *id);


#endif
