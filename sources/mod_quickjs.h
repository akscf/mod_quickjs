/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <switch.h>
#include <switch_stun.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#define MOD_VERSION "1.0"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_do_kill;
    uint32_t                id;
    uint32_t                tx_sem;
    const char              *session_id;
    char                    *args;
    void                    *script;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_core_session_t   *session;
    JSContext               *ctx;
} script_instance_t;

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_do_kill;
    uint32_t                id;
    uint32_t                tx_sem;
    uint32_t                instances;
    switch_size_t           code_length;
    char                    *name;
    char                    *path;
    char                    *code;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_inthash_t        *instances_map;
} script_t;

void ctx_dump_error(script_t *script, script_instance_t *instance, JSContext *ctx);

// --------------------------------------------------------------------------------------------------------
// session
typedef struct {
    uint8_t                 fl_hup_auto;
    uint8_t                 fl_hup_hook;
    switch_core_session_t   *session;
    JSValue                 on_hangup;
    JSContext               *ctx;
} js_session_t;

JSClassID js_seesion_class_get_id();
void js_session_class_register_rt(JSRuntime *rt);
switch_status_t js_session_class_register_ctx(JSContext *ctx, JSValue global_obj);
JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session);
JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// dtmf
typedef struct {
    switch_dtmf_t       *dtmf;
} js_dtmf_t;

JSClassID js_dtmf_class_get_id();
void js_dtmf_class_register_rt(JSRuntime *rt);
switch_status_t js_dtmf_class_register_ctx(JSContext *ctx, JSValue global_obj);
JSValue js_dtmf_object_create(JSContext *ctx, switch_dtmf_t *dtmf);

// event
typedef struct {
    switch_event_t      *event;
} js_event_t;

JSClassID js_event_class_get_id();
void js_event_class_register_rt(JSRuntime *rt);
switch_status_t js_event_class_register_ctx(JSContext *ctx, JSValue global_obj);
JSValue js_event_object_create(JSContext *ctx, switch_event_t *event);

// file handle
typedef struct {
    uint8_t                 fl_closed;
    uint8_t                 fl_auto_close;
    switch_core_session_t   *session;
    switch_file_handle_t    *fh;
} js_file_handle_t;

JSClassID js_file_handle_class_get_id();
void js_file_handle_class_register_rt(JSRuntime *rt);
switch_status_t js_file_handle_class_register_ctx(JSContext *ctx, JSValue global_obj);
JSValue js_file_handle_object_create(JSContext *ctx, switch_file_handle_t *fh, switch_core_session_t *session);

// file i/o
typedef struct {
    uint32_t                flags;
    int32_t                 bufsize;
    switch_size_t           buflen;
    char                    *path;
    char                    *buf;
    switch_file_t           *fd;
    switch_memory_pool_t    *pool;
} js_fileio_t;

JSClassID js_fileio_class_get_id();
void js_fileio_class_register_rt(JSRuntime *rt);
switch_status_t js_fileio_class_register_ctx(JSContext *ctx, JSValue global_obj);

#endif
