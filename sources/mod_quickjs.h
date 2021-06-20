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

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_do_kill;
    uint8_t                 fl_async;
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
    JSRuntime               *rt;
} script_t;

#endif
