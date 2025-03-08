/**
 * (C)2025 aks
 * https://github.com/akscf/
 **/
#include "js_dbh.h"

#define CLASS_NAME      "DBH"
#define PROP_DSN        1
#define PROP_CONNECTED  2
#define PROP_LAST_ERROR 3

#define DBH_SANITY_CHECK()  if (!js_dbh) { return JS_ThrowTypeError(ctx, "DBH is not initialized"); }
#define CONN_SANITY_CHECK() if (!js_dbh->dbh) { return JS_ThrowTypeError(ctx, "No database connection"); }

typedef struct {
    js_dbh_t        *js_dbh;
    JSContext       *ctx;
    JSValue         function;
    JSValue         arg;
} query_callback_t;

static void js_dbh_finalizer(JSRuntime *rt, JSValue val);

// void *pArg, int argc, char **argv, char **columnNames
static int xxx_query_callback(void *pArg, int argc, char **argv, char **cargv) {
    query_callback_t *qcb = (query_callback_t *)pArg;
    JSContext *ctx = qcb->ctx;
    JSValue args[1] = { 0 };
    JSValue ret_val;
    JSValue row_data;
    int rr = 0;

    row_data = JS_NewObject(ctx);
    for(int i = 0; i < argc; i++) {
        JS_SetPropertyStr(ctx, row_data, switch_str_nil(cargv[i]), JS_NewString(ctx, switch_str_nil(argv[i])));
    }
    args[0] = row_data;

    ret_val = JS_Call(ctx, qcb->function, JS_UNDEFINED, 1, (JSValueConst *) args);
    if(JS_IsException(ret_val)) {
        js_ctx_dump_error(NULL, ctx);
        JS_ResetUncatchableError(ctx);
    } else if(JS_IsBool(ret_val)) {
        rr = !JS_ToBool(ctx, ret_val);
    }

    JS_FreeValue(ctx, row_data);
    JS_FreeValue(ctx, ret_val);

    return rr;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_dbh_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));

    if(!js_dbh) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_DSN: {
            return JS_NewString(ctx, js_dbh->dsn);
        }
        case PROP_CONNECTED: {
            return js_dbh->dbh ? JS_TRUE : JS_FALSE;
        }
        case PROP_LAST_ERROR: {
            return zstr(js_dbh->last_error) ? JS_NULL : JS_NewString(ctx, js_dbh->last_error);
        }

    }

    return JS_UNDEFINED;
}

static JSValue js_dbh_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));

    return JS_FALSE;
}

// testReactive(testSql, dropSql, reactiveSql)
static JSValue js_dbh_test_reactive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));
    const char *test_sql, *drop_sql, *reactive_sql;
    JSValue result = JS_FALSE;

    DBH_SANITY_CHECK();
    CONN_SANITY_CHECK();

    if(argc < 3 || QJS_IS_NULL(argv[0]) || QJS_IS_NULL(argv[2])) {
        return JS_ThrowTypeError(ctx, "testReactive(testSql, dropSql, reactiveSql)");
    }

    test_sql = JS_ToCString(ctx, argv[0]);
    drop_sql = QJS_IS_NULL(argv[1]) ? NULL : JS_ToCString(ctx, argv[1]);
    reactive_sql = QJS_IS_NULL(argv[2]) ? NULL : JS_ToCString(ctx, argv[2]);

    if(switch_cache_db_test_reactive(js_dbh->dbh, test_sql, drop_sql, reactive_sql) == SWITCH_TRUE) {
        result = JS_TRUE;
    }

    JS_FreeCString(ctx, test_sql);
    JS_FreeCString(ctx, drop_sql);
    JS_FreeCString(ctx, reactive_sql);
    return result;
}

// affectedRows()
static JSValue js_dbh_affected_rows(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));

    DBH_SANITY_CHECK();
    CONN_SANITY_CHECK();

    return JS_NewInt32(ctx, switch_cache_db_affected_rows(js_dbh->dbh));
}

// loadExtension(name)
static JSValue js_dbh_load_extensions(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));
    const char *ext_name;
    JSValue result = JS_FALSE;

    DBH_SANITY_CHECK();
    CONN_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "loadExtension(name)");
    }

    ext_name = JS_ToCString(ctx, argv[0]);
    if(zstr(ext_name)) { return JS_FALSE; }

    if(!switch_cache_db_load_extension(js_dbh->dbh, ext_name)) {
        result = JS_TRUE;
    }

    JS_FreeCString(ctx, ext_name);
    return result;
}

static JSValue js_dbh_clear_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));

    DBH_SANITY_CHECK();

    switch_safe_free(js_dbh->last_error);
    js_dbh->last_error = NULL;

    return JS_TRUE;
}

// execQuery(query, [callback, callbackData])
static JSValue js_dbh_exec_query(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = JS_GetOpaque2(ctx, this_val, js_dbh_get_classid(ctx));
    query_callback_t qcb = { 0 };
    const char *query;
    JSValue result = JS_FALSE;

    DBH_SANITY_CHECK();
    CONN_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "execQuery(query, [callback, callbackData])");
    }

    query = JS_ToCString(ctx, argv[0]);
    if(zstr(query)) { return JS_FALSE; }

    if(argc > 1 && JS_IsFunction(ctx, argv[1])) {
        qcb.ctx = ctx;
        qcb.js_dbh = js_dbh;
        qcb.function = argv[1];
        qcb.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);

        if(switch_cache_db_execute_sql_callback(js_dbh->dbh, query, xxx_query_callback, &qcb, &js_dbh->last_error) == SWITCH_STATUS_SUCCESS) {
            result = JS_TRUE;
        }
    } else {
        if(switch_cache_db_execute_sql(js_dbh->dbh, (char *)query, &js_dbh->last_error) == SWITCH_STATUS_SUCCESS) {
            result = JS_TRUE;
        }
    }

    JS_FreeCString(ctx, query);
    return result;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_dbh_class = {
    CLASS_NAME,
    .finalizer = js_dbh_finalizer,
};

static const JSCFunctionListEntry js_dbh_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("dsn", js_dbh_property_get, js_dbh_property_set, PROP_DSN),
    JS_CGETSET_MAGIC_DEF("error", js_dbh_property_get, js_dbh_property_set, PROP_LAST_ERROR),
    JS_CGETSET_MAGIC_DEF("isConnected", js_dbh_property_get, js_dbh_property_set, PROP_CONNECTED),
    //
    JS_CFUNC_DEF("loadExtension", 1, js_dbh_load_extensions),
    JS_CFUNC_DEF("testReactive", 1, js_dbh_test_reactive),
    JS_CFUNC_DEF("affectedRows", 1, js_dbh_affected_rows),
    JS_CFUNC_DEF("clerError", 1, js_dbh_clear_error),
    JS_CFUNC_DEF("execQuery", 1, js_dbh_exec_query)
};

static void js_dbh_finalizer(JSRuntime *rt, JSValue val) {
    js_dbh_t *js_dbh = JS_GetOpaque(val, js_dbh_get_classid2(rt));
    switch_memory_pool_t *pool = (js_dbh ? js_dbh->pool : NULL);

    if(!js_dbh) {
        return;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-dbh-finalizer: js_dbh=%p, dbh=%p\n", js_dbh, js_dbh->dbh);
#endif

    if(js_dbh->dbh) {
        switch_cache_db_release_db_handle(&js_dbh->dbh);
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_dbh);
}

// new DBH(dsn)
static JSValue js_dbh_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    js_dbh_t *js_dbh = NULL;
    const char *dsn = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_cache_db_handle_t *dbh = NULL;
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "new DBH(dsn)");
    }

    dsn = JS_ToCString(ctx, argv[0]);
    if(zstr(dsn)) {
        err = JS_ThrowTypeError(ctx, "new DBH(dsn)");
        goto fail;
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto fail;
    }

    if(switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "switch_cache_db_get_db_handle_dsn()\n");
        dbh = NULL;
    }

    js_dbh = js_mallocz(ctx, sizeof(js_dbh_t));
    if(!js_dbh) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        goto fail;
    }

    js_dbh->pool = pool;
    js_dbh->dbh = dbh;
    js_dbh->dsn = switch_core_strdup(pool, dsn);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_dbh_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_dbh);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-dbh-constructor: js_dbh=%p, dbh=%p\n", js_dbh, dbh);
#endif

    JS_FreeCString(ctx, dsn);
    return obj;
fail:
    if(dbh) {
        switch_cache_db_release_db_handle(&dbh);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_dbh) {
        js_free(ctx, js_dbh);
    }

    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, dsn);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_dbh_get_classid2(JSRuntime *rt) {
    script_t *script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);
    return script->class_id_dbh;
}
JSClassID js_dbh_get_classid(JSContext *ctx) {
    return  js_dbh_get_classid2(JS_GetRuntime(ctx));
}


switch_status_t js_dbh_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id) {
    JSValue obj_proto, obj_class;
    script_t *script = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    switch_assert(script);

    if(JS_IsRegisteredClass(JS_GetRuntime(ctx), class_id)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Class with id (%d) already registered!\n", class_id);
        return SWITCH_STATUS_FALSE;
    }

    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, &js_dbh_class);
    script->class_id_dbh = class_id;

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Class registered [%s / %d]\n", CLASS_NAME, class_id);
#endif

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_dbh_proto_funcs, ARRAY_SIZE(js_dbh_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_dbh_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}
