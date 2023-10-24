/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_COREDB_H
#define JS_COREDB_H
#include "mod_quickjs.h"

typedef struct {
    char                    *name;
    switch_core_db_t        *db;
    switch_core_db_stmt_t   *stmt;
    switch_memory_pool_t    *pool;
    JSContext               *ctx;
    JSValue                 callback;
} js_coredb_t;

JSClassID js_coredb_get_classid(JSContext *ctx);
switch_status_t js_coredb_class_register(JSContext *ctx, JSValue global_obj);

#endif


