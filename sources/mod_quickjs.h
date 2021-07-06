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
    switch_core_session_t   *session;
    uint8_t                 fl_hup;
} js_session_t;

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


void js_session_class_init_rt(JSRuntime *rt);
JSClassID js_seesion_get_class_id();
switch_status_t js_session_class_register_ctx(JSContext *ctx, JSValue global_obj);

#endif
