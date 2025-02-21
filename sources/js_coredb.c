/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include "js_coredb.h"

#define CLASS_NAME               "CoreDB"
#define PROP_PATH                0

#define DB_SANITY_CHECK() if (!js_coredb || !js_coredb->db) { \
           return JS_ThrowTypeError(ctx, "DB is not initialized"); \
        }
#define DB_SANITY_CHECK_STATEMENT() if (!js_coredb->stmt) { \
           return JS_ThrowTypeError(ctx, "No active statement"); \
        }

static void js_coredb_finalizer(JSRuntime *rt, JSValue val);
static int db_callback(void *udata, int argc, char **argv, char **columnNames);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_coredb_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));

    if(!js_coredb) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_PATH: {
            return JS_NewString(ctx, js_coredb->name);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_coredb_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));

    return JS_FALSE;
}

static JSValue js_coredb_table_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    switch_core_db_stmt_t *stmt = NULL;
    const char *table_name = NULL;
    char *sql = NULL;
    JSValue ret_value = JS_FALSE;
    int wait = 5000, rows = 0, ret = 0;

    DB_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: tableName");
    }

    table_name = JS_ToCString(ctx, argv[0]);

    sql = switch_mprintf("SELECT name FROM sqlite_master WHERE type='table' AND name='%s'", table_name);
    if(switch_core_db_prepare(js_coredb->db, sql, -1, &stmt, 0) != 0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL error: %s\n", switch_core_db_errmsg(js_coredb->db));
        goto out;
    }

    while(wait > 0) {
        ret = switch_core_db_step(stmt);
        if(ret == SWITCH_CORE_DB_BUSY) {
            wait--; switch_cond_next();
            continue;
        }
        if(ret == SWITCH_CORE_DB_ROW) {
            rows++;
            break;
        }
        switch_core_db_finalize(stmt);
        break;
    }

    ret_value = (rows > 0 ? JS_TRUE : JS_FALSE);
out:
    JS_FreeCString(ctx, table_name);
    switch_safe_free(sql);
    return ret_value;
}

static JSValue js_coredb_exec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    switch_core_db_callback_func_t cbfnc = NULL;
    int count = 0;
    const char *sqltxt = NULL;
    char *err = NULL;
    void *arg = NULL;

    DB_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: query");
    }

    sqltxt = JS_ToCString(ctx, argv[0]);

    if(!JS_IsUndefined(js_coredb->callback)) {
        js_coredb->callback = JS_UNDEFINED;
    }
    if(argc > 1) {
        if(JS_IsFunction(ctx, argv[1])) {
            js_coredb->ctx = ctx;
            js_coredb->callback = argv[1];
            cbfnc = db_callback;
            arg = js_coredb;
        }
    }

    switch_core_db_exec(js_coredb->db, sqltxt, cbfnc, arg, &err);
    if(err) {
        JSValue ret = JS_ThrowTypeError(ctx, "SQL error: %s", err);
        switch_core_db_free(err);
        JS_FreeCString(ctx, sqltxt);
        return ret;
    }

    count = switch_core_db_changes(js_coredb->db);
    JS_FreeCString(ctx, sqltxt);

    return JS_NewInt32(ctx, count);
}

static JSValue js_coredb_prepare(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    int ret = 0;
    const char *sqltxt = NULL;

    DB_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(js_coredb->stmt) {
        switch_core_db_finalize(js_coredb->stmt);
        js_coredb->stmt = NULL;
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: query");
    }

    sqltxt = JS_ToCString(ctx, argv[0]);

    ret = switch_core_db_prepare(js_coredb->db, sqltxt, -1, &js_coredb->stmt, 0);
    JS_FreeCString(ctx, sqltxt);

    if(!ret) {
        return JS_TRUE;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL error: %s\n", switch_core_db_errmsg(js_coredb->db));
    return JS_FALSE;
}

static JSValue js_coredb_fetch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    int cols = 0, i = 0;
    JSValue row_data;

    DB_SANITY_CHECK();

    if(!js_coredb->stmt) {
        return JS_ThrowTypeError(ctx, "No active statement");
    }

    cols = switch_core_db_column_count(js_coredb->stmt);
    row_data = JS_NewObject(ctx);

    for(i = 0; i < cols; i++) {
        const char *var = (char *) switch_core_db_column_name(js_coredb->stmt, i);
        const char *val = (char *) switch_core_db_column_text(js_coredb->stmt, i);

        JS_SetPropertyStr(ctx, row_data, var, JS_NewString(ctx, val));
    }

    return row_data;
}

static JSValue js_coredb_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    int wait = 5000, success = 0;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    while(wait > 0) {
        int ret = switch_core_db_step(js_coredb->stmt);

        if(ret == SWITCH_CORE_DB_BUSY) { switch_cond_next(); wait--; continue; }
        if(ret == SWITCH_CORE_DB_ROW)  { success = 1; break; }
        if(ret == SWITCH_CORE_DB_DONE) { success = 0; break; }

        if(switch_core_db_finalize(js_coredb->stmt) != SWITCH_CORE_DB_OK) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL error: %s\n", switch_core_db_errmsg(js_coredb->db));
        }

        js_coredb->stmt = NULL;
        break;
    }

    return (success ? JS_TRUE : JS_FALSE);
}

static JSValue js_coredb_step(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    int wait = 5000, success = 0;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    while(wait > 0) {
        int ret = switch_core_db_step(js_coredb->stmt);

        if(ret == SWITCH_CORE_DB_BUSY) { switch_cond_next(); wait--; continue; }
        if(ret == SWITCH_CORE_DB_DONE) { success = 1; break; }

        if(switch_core_db_finalize(js_coredb->stmt) != SWITCH_CORE_DB_OK) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL error: %s\n", switch_core_db_errmsg(js_coredb->db));
        }

        js_coredb->stmt = NULL;
        break;
    }

    return JS_FALSE;
}


static JSValue js_coredb_bind_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    const char *val = NULL;
    int idx = 0, err = 0;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    JS_ToUint32(ctx, &idx, argv[0]);
    if(idx <= 0) {
        return JS_FALSE;
    }

    err = switch_core_db_bind_text(js_coredb->stmt, idx, val, -1, SWITCH_CORE_DB_STATIC);
    JS_FreeCString(ctx, val);

    return (err == 0 ? JS_TRUE : JS_FALSE);
}

static JSValue js_coredb_bind_int(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_coredb_t *js_coredb = JS_GetOpaque2(ctx, this_val, js_coredb_get_classid(ctx));
    int idx = 0, val = 0, err = 0;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    JS_ToUint32(ctx, &idx, argv[0]);
    if(idx <= 0) {
        return JS_FALSE;
    }

    JS_ToUint32(ctx, &val, argv[1]);

    err = switch_core_db_bind_int(js_coredb->stmt, idx, val);

    return (err == 0 ? JS_TRUE : JS_FALSE);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// callbacks
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static int db_callback(void *udata, int argc, char **argv, char **columnNames) {
    js_coredb_t *js_coredb = (js_coredb_t *) udata;
    JSContext *ctx = (js_coredb ? js_coredb->ctx : NULL);
    JSValue args[1] = { 0 };
    JSValue ret_val;
    JSValue row_data;
    int i = 0;

    if(!js_coredb) {
        return 0;
    }
    if(JS_IsUndefined(js_coredb->callback)) {
        return 0;
    }

    row_data = JS_NewObject(ctx);
    for(i = 0; i < argc; i++) {
        JS_SetPropertyStr(ctx, row_data, columnNames[i], JS_NewString(ctx, argv[i]));
    }
    args[0] = row_data;

    ret_val = JS_Call(ctx, js_coredb->callback, JS_UNDEFINED, 1, (JSValueConst *) args);
    if(JS_IsException(ret_val)) {
        js_ctx_dump_error(NULL, ctx);
        JS_ResetUncatchableError(ctx);
    }

    JS_FreeValue(ctx, row_data);
    JS_FreeValue(ctx, ret_val);

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_coredb_class = {
    CLASS_NAME,
    .finalizer = js_coredb_finalizer,
};

static const JSCFunctionListEntry js_coredb_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("path", js_coredb_property_get, js_coredb_property_set, PROP_PATH),
    //
    JS_CFUNC_DEF("exec", 1, js_coredb_exec),
    JS_CFUNC_DEF("next", 0, js_coredb_next),
    JS_CFUNC_DEF("step", 0, js_coredb_step),
    JS_CFUNC_DEF("fetch", 0, js_coredb_fetch),
    JS_CFUNC_DEF("prepare", 2, js_coredb_prepare),
    JS_CFUNC_DEF("bindText", 2, js_coredb_bind_text),
    JS_CFUNC_DEF("bindInt", 2, js_coredb_bind_int),
    JS_CFUNC_DEF("tableExists", 1, js_coredb_table_exists),
};

static void js_coredb_finalizer(JSRuntime *rt, JSValue val) {
    js_coredb_t *js_coredb = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));
    switch_memory_pool_t *pool = (js_coredb ? js_coredb->pool : NULL);

    if(!js_coredb) {
        return;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-coredb-finalizer: js_coredb=%p, db=%p\n", js_coredb, js_coredb->db);
#endif

    if(js_coredb->stmt) {
        switch_core_db_finalize(js_coredb->stmt);
        js_coredb->stmt = NULL;
    }

    if(js_coredb->db) {
        switch_core_db_close(js_coredb->db);
        js_coredb->db = NULL;
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_coredb);
}

static JSValue js_coredb_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_coredb_t *js_coredb = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_core_db_t *db = NULL;
    const char *dbname = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: dbname");
    }

    dbname = JS_ToCString(ctx, argv[0]);

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto fail;
    }

    db = switch_core_db_open_file(dbname);
    if(!db) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't open DB\n");
        goto fail;
    }

    js_coredb = js_mallocz(ctx, sizeof(js_coredb_t));
    if(!js_coredb) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        goto fail;
    }
    js_coredb->db = db;
    js_coredb->pool = pool;
    js_coredb->name = switch_core_strdup(pool, dbname);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_coredb_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_coredb);
    JS_FreeCString(ctx, dbname);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-coredb-constructor: js-coredb=%p, db=%p\n", js_coredb, js_coredb->db);
#endif

    return obj;
fail:
    if(db) {
        switch_core_db_close(db);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_coredb) {
        js_free(ctx, js_coredb);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, dbname);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_coredb_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_coredb_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_coredb_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_coredb_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_coredb_proto_funcs, ARRAY_SIZE(js_coredb_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_coredb_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}
