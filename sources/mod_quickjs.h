/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <switch.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#define MOD_VERSION "1.0"

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_do_kill;
    uint32_t                mode;
    uint32_t                refs;
    uint32_t                tx_sem;
    char                    *name;
    char                    *path;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    //JSRuntime               *rt;
    //JSContext               *ctx;

} script_t;



#endif
