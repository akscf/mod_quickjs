/**
 * (C)2025 aks
 * https://github.com/akscf/
 **/
#ifndef JS_DBH_H
#define JS_DBH_H
#include "mod_quickjs.h"

typedef struct {
    switch_memory_pool_t        *pool;
    switch_cache_db_handle_t    *dbh;
    char                        *dsn;
    char                        *last_error;
} js_dbh_t;

JSClassID js_dbh_get_classid(JSContext *ctx);
switch_status_t js_dbh_class_register(JSContext *ctx, JSValue global_obj);


#endif

