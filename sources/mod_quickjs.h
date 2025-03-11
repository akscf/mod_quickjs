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
#include <dlfcn.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#ifndef true
 #define true SWITCH_TRUE
#endif
#ifndef false
 #define false SWITCH_FALSE
#endif

#define QJS_IS_NULL(jsV)    (JS_IsNull(jsV) || JS_IsUndefined(jsV) || JS_IsUninitialized(jsV))
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))
#define JID_NONE            0x0

#define MOD_VERSION         "v1.7.5"
#define MOD_RT_TYPE         "opensource"

#define MOD_QUICKJS_DEBUG

typedef JSModuleDef *(JSInitModuleFunc)(JSContext *ctx, const char *module_name);

typedef struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts_map;
    switch_hash_t           *scripts_map;
    uint32_t                active_threads;
    size_t                  cfg_rt_mem_limit;
    size_t                  cfg_rt_stk_size;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
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
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSRuntime               *rt;
    void                    *opaque;
    // builtin classes
    JSClassID               class_id_codec;
    JSClassID               class_id_coredb;
    JSClassID               class_id_curl;
    JSClassID               class_id_dbh;
    JSClassID               class_id_event;
    JSClassID               class_id_eventhandler;
    JSClassID               class_id_file;
    JSClassID               class_id_filehandle;
    JSClassID               class_id_session;
    JSClassID               class_id_socket;
    JSClassID               class_id_xml;
} script_t;

typedef struct {
    JSClassID   id;
} class_id_t;

/* utils.c */
char *safe_pool_strdup(switch_memory_pool_t *pool, const char *str);
uint8_t *safe_pool_bufdup(switch_memory_pool_t *pool, uint8_t *buffer, switch_size_t len);

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data);
void thread_finished();

void js_ctx_dump_error(script_t *script, JSContext *ctx);

switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool);
uint32_t script_sem_take(script_t *script);
void script_sem_release(script_t *script);
void script_wait_unlock(script_t *script);
script_t *script_lookup(char *id);

/* quickjs */
int has_suffix(const char *str, const char *suffix);

#endif
