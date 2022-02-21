/**
 * ODBC
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "mod_quickjs.h"

#ifdef FS_MOD_ENABLE_ODBC

#define CLASS_NAME              "ODBC"
#define PROP_DSN                0
#define PROP_USERNAME           1
#define PROP_PASSWORD           2
#define PROP_NUM_ROWS           3
#define PROP_NUM_COLS           4
#define PROP_CONNECTED          5


#define DB_SANITY_CHECK() if (!js_odbc || !js_odbc->db) { \
           return JS_ThrowTypeError(ctx, "ODBC not initialized"); \
        }
#define DB_SANITY_CHECK_STATEMENT() if (!js_odbc->stmt) { \
           return JS_ThrowTypeError(ctx, "No active statement"); \
        }

static void js_odbc_finalizer(JSRuntime *rt, JSValue val);
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_odbc_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));

    if(!js_odbc) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_DSN: {
            return JS_NewString(ctx, js_odbc->dsn);
        }
        case PROP_USERNAME: {
            return js_odbc->username ? JS_NewString(ctx, js_odbc->username) : JS_UNDEFINED;
        }
        case PROP_PASSWORD: {
            return js_odbc->password ? JS_NewString(ctx, js_odbc->password) : JS_UNDEFINED;
        }
        case PROP_CONNECTED: {
            if(js_odbc->db && switch_odbc_handle_get_state(js_odbc->db) == SWITCH_ODBC_STATE_CONNECTED) {
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_NUM_ROWS: {
            SQLLEN res = 0;
            if(js_odbc->db && js_odbc->stmt && switch_odbc_handle_get_state(js_odbc->db) == SWITCH_ODBC_STATE_CONNECTED) {
                SQLRowCount(js_odbc->stmt, &res);
            }
            return JS_NewInt32(ctx, res);
        }
        case PROP_NUM_COLS: {
            SQLSMALLINT res = 0;
            if(js_odbc->db && js_odbc->stmt && switch_odbc_handle_get_state(js_odbc->db) == SWITCH_ODBC_STATE_CONNECTED) {
                SQLNumResultCols(js_odbc->stmt, &res);
            }
            return JS_NewInt32(ctx, res);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_odbc_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));

    return JS_FALSE;
}

static JSValue js_odbc_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));
    switch_odbc_status_t st;

    DB_SANITY_CHECK();

    st = switch_odbc_handle_connect(js_odbc->db);
    return (st == SWITCH_ODBC_SUCCESS  ? JS_TRUE : JS_FALSE);
}

static JSValue js_odbc_disconnect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));

    DB_SANITY_CHECK();

    if(switch_odbc_handle_get_state(js_odbc->db) == SWITCH_ODBC_STATE_CONNECTED) {
        if(js_odbc->stmt) {
            SQLFreeHandle(SQL_HANDLE_STMT, js_odbc->stmt);
            js_odbc->stmt = NULL;
        }
    }
    switch_odbc_handle_disconnect(js_odbc->db);

    return JS_TRUE;
}

static JSValue js_odbc_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));
    JSValue ret_value = JS_FALSE;
    SQLHSTMT stmt;
    const char *sql = NULL;

    DB_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    sql = JS_ToCString(ctx, argv[0]);
    if(zstr(sql)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: sql");
    }

    if(switch_odbc_handle_get_state(js_odbc->db) != SWITCH_ODBC_STATE_CONNECTED) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
        goto out;
    }

    if(switch_odbc_handle_exec(js_odbc->db, sql, &stmt, NULL) != SWITCH_ODBC_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] Execute failed for: %s\n", sql);
        goto out;
    }

    ret_value = JS_TRUE;
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);

out:
    JS_FreeCString(ctx, sql);
    return ret_value;
}

static JSValue js_odbc_exec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));
    JSValue ret_value = JS_FALSE;
    const char *sql = NULL;

    DB_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    sql = JS_ToCString(ctx, argv[0]);
    if(zstr(sql)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: sql");
    }

    if(switch_odbc_handle_get_state(js_odbc->db) != SWITCH_ODBC_STATE_CONNECTED) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
        goto out;
    }

    if(js_odbc->stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, js_odbc->stmt);
        js_odbc->stmt = NULL;
    }

    if(switch_odbc_handle_exec(js_odbc->db, sql, &js_odbc->stmt, NULL) != SWITCH_ODBC_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ODBC] query failed: %s\n", sql);
        goto out;
    }

    ret_value = JS_TRUE;

out:
    JS_FreeCString(ctx, sql);
    return ret_value;
}


static JSValue js_odbc_next_row(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));
    int res = 0;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    if(switch_odbc_handle_get_state(js_odbc->db) != SWITCH_ODBC_STATE_CONNECTED) {
        return JS_FALSE;
    }

    if(!js_odbc->stmt) {
        return JS_FALSE;
    }

    res = SQLFetch(js_odbc->stmt);
    return (res == SQL_SUCCESS ? JS_TRUE : JS_FALSE);
}

static JSValue js_odbc_get_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_odbc_t *js_odbc = JS_GetOpaque2(ctx, this_val, js_odbc_get_classid(ctx));
    SQLSMALLINT cols = 0, i = 0;
    JSValue row_data;

    DB_SANITY_CHECK();
    DB_SANITY_CHECK_STATEMENT();

    if(switch_odbc_handle_get_state(js_odbc->db) != SWITCH_ODBC_STATE_CONNECTED) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Database is not connected!\n");
        return JS_UNDEFINED;
    }

    if(!js_odbc->stmt) {
        return JS_UNDEFINED;
    }

    if(SQLNumResultCols(js_odbc->stmt, &cols) != SQL_SUCCESS) {
        return JS_UNDEFINED;
    }

    row_data = JS_NewObject(ctx);
    for(i = 1; i <= cols; i++) {
        SQLSMALLINT NameLength, DataType, DecimalDigits, Nullable;
        SQLULEN ColumnSize;
        SQLCHAR name[1024] = "";
        SQLCHAR *data = js_odbc->colbuf;
        SQLCHAR *esc = NULL;

        SQLDescribeCol(js_odbc->stmt, i, name, sizeof(name), &NameLength, &DataType, &ColumnSize, &DecimalDigits, &Nullable);
        SQLGetData(js_odbc->stmt, i, SQL_C_CHAR, js_odbc->colbuf, js_odbc->colbufsz, NULL);

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "COLMN: %s => %s\n", (char *)name, (char *)data);
        JS_SetPropertyStr(ctx, row_data, name, JS_NewString(ctx, data));
    }

    return row_data;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_odbc_class = {
    CLASS_NAME,
    .finalizer = js_odbc_finalizer,
};

static const JSCFunctionListEntry js_odbc_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("dsn", js_odbc_property_get, js_odbc_property_set, PROP_DSN),
    JS_CGETSET_MAGIC_DEF("username", js_odbc_property_get, js_odbc_property_set, PROP_USERNAME),
    JS_CGETSET_MAGIC_DEF("password", js_odbc_property_get, js_odbc_property_set, PROP_PASSWORD),
    JS_CGETSET_MAGIC_DEF("numRows", js_odbc_property_get, js_odbc_property_set, PROP_NUM_ROWS),
    JS_CGETSET_MAGIC_DEF("numCols", js_odbc_property_get, js_odbc_property_set, PROP_NUM_COLS),
    JS_CGETSET_MAGIC_DEF("isConnected", js_odbc_property_get, js_odbc_property_set, PROP_CONNECTED),
    //
    JS_CFUNC_DEF("connect", 0, js_odbc_connect),
    JS_CFUNC_DEF("disconnect", 0, js_odbc_disconnect),
    JS_CFUNC_DEF("query", 1, js_odbc_exec),
    JS_CFUNC_DEF("exec", 1,  js_odbc_exec),
    JS_CFUNC_DEF("execute", 1, js_odbc_execute),
    JS_CFUNC_DEF("nextRow", 0, js_odbc_next_row),
    JS_CFUNC_DEF("getData", 0, js_odbc_get_data),
};

static void js_odbc_finalizer(JSRuntime *rt, JSValue val) {
    js_odbc_t *js_odbc = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));
    switch_memory_pool_t *pool = (js_odbc ? js_odbc->pool : NULL);

    if(!js_odbc) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-odbc-finalizer: js_odbc=%p, db=%p\n", js_odbc, js_odbc->db);

    if(js_odbc->stmt) {
        SQLFreeHandle(SQL_HANDLE_STMT, js_odbc->stmt);
        js_odbc->stmt = NULL;
    }

    if(js_odbc->db) {
        switch_odbc_handle_destroy(&js_odbc->db);
        js_odbc->db = NULL;
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_odbc);
}

static JSValue js_odbc_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_odbc_t *js_odbc = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_odbc_handle_t *db = NULL;
    const char *dsn = NULL, *username = NULL, *password = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    dsn = JS_ToCString(ctx, argv[0]);
    if(zstr(dsn)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: dsn");
    }
    if(argc > 1) {
        username = JS_ToCString(ctx, argv[1]);
    }
    if(argc > 2) {
        password = JS_ToCString(ctx, argv[2]);
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        goto fail;
    }

    db = switch_odbc_handle_new(dsn, username, password);
    if(!db) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't init odbc\n");
        goto fail;
    }

    js_odbc = js_mallocz(ctx, sizeof(js_odbc));
    if(!js_odbc) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }

    js_odbc->db = db;
    js_odbc->pool = pool;
    js_odbc->dsn = switch_core_strdup(pool, dsn);
    js_odbc->username = (username ? switch_core_strdup(pool, username) : NULL);
    js_odbc->password = (password ? switch_core_strdup(pool, password) : NULL);
    js_odbc->colbufsz = 1024;
    js_odbc->colbuf = switch_core_alloc(pool, js_odbc->colbufsz);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_odbc_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_odbc);

    JS_FreeCString(ctx, dsn);
    JS_FreeCString(ctx, username);
    JS_FreeCString(ctx, password);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-odbc-constructor: js-odbc=%p, db=%p\n", js_odbc, js_odbc->db);

    return obj;

fail:
    if(db) {
        switch_odbc_handle_destroy(&db);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_odbc) {
        js_free(ctx, js_odbc);
    }

    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, dsn);
    JS_FreeCString(ctx, username);
    JS_FreeCString(ctx, password);

    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_odbc_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_odbc_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_odbc_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_odbc_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_odbc_proto_funcs, ARRAY_SIZE(js_odbc_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_odbc_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

#endif // FS_MOD_ENABLE_ODBC

