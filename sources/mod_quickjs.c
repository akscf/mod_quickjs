/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

static struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts;
    switch_inthash_t        *scripts_map;
    switch_mutex_t          *mutex_rt;
    JSRuntime               *qjs_rt;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    int                     active_threads;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj);

static script_t *script_lookup(char *name);
static uint32_t script_sem_take(script_t *script);
static script_instance_t *instance_lookup(script_t *script, uint32_t id);
static void script_sem_release(script_t *script);
static uint32_t script_instance_sem_take(script_instance_t *instance);
static void script_instance_sem_release(script_instance_t *instance);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ctx_dump_error(script_t *script, script_instance_t *instance, JSContext *ctx) {
    JSValue exception_val = JS_GetException(ctx);

    if(JS_IsError(ctx, exception_val)) {
        JSValue stk_val = JS_GetPropertyStr(ctx, exception_val, "stack");
        const char *err_str = JS_ToCString(ctx, exception_val);
        const char *stk_str = (JS_IsUndefined(stk_val) ? NULL : JS_ToCString(ctx, stk_val));

        if(script && instance) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "(%X:%s)\n%s %s\n", instance->id, script->name, (err_str ? err_str : "[exception]"), stk_str);
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s %s\n", (err_str ? err_str : "[exception]"), stk_str);
        }

        if(err_str) JS_FreeCString(ctx, err_str);
        if(stk_str) JS_FreeCString(ctx, stk_str);

        JS_FreeValue(ctx, stk_val);
    }

    JS_FreeValue(ctx, exception_val);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    const char *file = __FILE__;
    int line = __LINE__;

    if(argc > 1) {
        const char *lvl_str, *msg_str;
        lvl_str = JS_ToCString(ctx, argv[0]);
        if(!zstr(lvl_str)) {
            level = switch_log_str2level(lvl_str);
        }
        if(level == SWITCH_LOG_INVALID) {
            level = SWITCH_LOG_DEBUG;
        }

        msg_str = JS_ToCString(ctx, argv[1]);
        switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s\n", msg_str);

        JS_FreeCString(ctx, lvl_str);
        JS_FreeCString(ctx, msg_str);
    } else if(argc > 0) {
        const char *msg_str;
        msg_str = JS_ToCString(ctx, argv[0]);

        if(!zstr(msg_str)) {
            switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s\n", msg_str);
        }

        JS_FreeCString(ctx, msg_str);
    }
    return JS_UNDEFINED;
}

static JSValue js_include(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ret_val = JS_FALSE;
    script_instance_t *script_instance = NULL;
    script_t *script = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_file_t *fd = NULL;
    switch_size_t flen = 0;
    const char *path = NULL;
    char *path_local = NULL;
    char *buf = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    script_instance = JS_GetContextOpaque(ctx);
    if(!script_instance) {
        return JS_ThrowTypeError(ctx, "Invalid runtime");
    }
    script = (script_t *)(script_instance ? script_instance->script : NULL);

    path = JS_ToCString(ctx, argv[0]);
    if(zstr(path)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: filename");
    }

    if(switch_file_exists(path, NULL) == SWITCH_STATUS_SUCCESS) {
        path_local = strdup(path);
    } else {
        path_local = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, path);
        if(switch_file_exists(path_local, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found: %s\n", path_local);
            goto out;
        }
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto out;
    }

    if(switch_file_open(&fd, path_local, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, pool) != SWITCH_STATUS_SUCCESS) {
        ret_val = JS_ThrowTypeError(ctx, "Couldn't open file: %s\n", path_local);
        goto out;
    }

    if((flen = switch_file_get_size(fd)) > 0) {
        if((buf = switch_core_alloc(pool, flen)) == NULL) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            goto out;
        }
        if(switch_file_read(fd, buf, &flen) != SWITCH_STATUS_SUCCESS) {
            ret_val = JS_ThrowTypeError(ctx, "Couldn't read file\n");
            goto out;
        }

        ret_val = JS_Eval(ctx, buf, flen, path_local, JS_EVAL_TYPE_GLOBAL);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(script, script_instance, ctx);
            JS_ResetUncatchableError(ctx);
        }

        JS_FreeValue(ctx, ret_val);
        ret_val = JS_TRUE;
    }
out:
    if(fd) {
        switch_file_close(fd);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    switch_safe_free(path_local);
    JS_FreeCString(ctx, path);
    return ret_val;
}

static JSValue js_msleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t msec = 0;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    JS_ToUint32(ctx, &msec, argv[0]);

    if(msec) {
        switch_yield(msec * 1000);
    }

    return JS_TRUE;
}

static JSValue js_global_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *var_str = NULL;
    const char *val_str = NULL;

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    var_str = JS_ToCString(ctx, argv[0]);
    val_str = JS_ToCString(ctx, argv[1]);

    switch_core_set_variable(var_str, val_str);

    JS_FreeCString(ctx, var_str);
    JS_FreeCString(ctx, val_str);

    return JS_TRUE;
}

static JSValue js_global_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *var_str;
    char *val = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    var_str = JS_ToCString(ctx, argv[0]);
    val = switch_core_get_variable(var_str);
    JS_FreeCString(ctx, var_str);

    if(val) {
        if(strcasecmp(val, "true") == 0) {
            return JS_TRUE;
        } else if(strcasecmp(val, "false") == 0) {
            return JS_FALSE;
        } else {
            return JS_NewString(ctx, val);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ret_val;
    const char *exit_code;

    if(!argc) {
        return JS_FALSE;
    }

    exit_code = JS_ToCString(ctx, argv[0]);
    if(zstr(exit_code)) {
        return JS_FALSE;
    }

    ret_val = JS_ThrowTypeError(ctx, "ERROR: %s", exit_code);
    JS_FreeCString(ctx, exit_code);

    return ret_val;
}

static JSValue js_system(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *cmd;
    int result;

    if(!argc) {
        return JS_FALSE;
    }

    cmd = JS_ToCString(ctx, argv[0]);
    if(zstr(cmd)) {
        return JS_UNDEFINED;
    }
    result = switch_system(cmd, SWITCH_TRUE);
    JS_FreeCString(ctx, cmd);

    return JS_NewInt32(ctx, result);
}

static JSValue js_api_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *api_str;
    const char *arg_str;
    script_instance_t *script_instance = NULL;
    switch_core_session_t *session = NULL;
    switch_stream_handle_t stream = { 0 };
    JSValue js_ret_val;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    api_str = JS_ToCString(ctx, argv[0]);
    arg_str = (argc > 1 ? JS_ToCString(ctx, argv[1]) : NULL);

    if(zstr(api_str)) {
        return JS_UNDEFINED;
    }

    script_instance = JS_GetContextOpaque(ctx);
    if(!script_instance) {
        return JS_ThrowTypeError(ctx, "Invalid runtime");
    }

    session = (script_instance ? script_instance->session : NULL);

    SWITCH_STANDARD_STREAM(stream);
    switch_api_execute(api_str, arg_str, session, &stream);
    js_ret_val = JS_NewString(ctx, switch_str_nil((char *) stream.data));

    switch_safe_free(stream.data);
    JS_FreeCString(ctx, api_str);
    JS_FreeCString(ctx, arg_str);

    return js_ret_val;
}

static JSValue js_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return js_session_ext_bridge(ctx, this_val, argc, argv);
}

static JSValue js_global_set_interrupt_handler(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_instance_t *script_instance = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    script_instance = JS_GetContextOpaque(ctx);
    if(!script_instance) {
        return JS_ThrowTypeError(ctx, "Invalid runtime");
    }

    if(JS_IsNull(argv[0])) {
        script_instance->int_handler = JS_UNDEFINED;
        return JS_TRUE;
    }

    if(JS_IsFunction(ctx, argv[0])) {
        script_instance->int_handler = argv[0];
        return JS_TRUE;
    }

    return JS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t name2id(char *name, uint32_t len) {
    return switch_crc32_8bytes((char *)name, len);
}

static uint32_t rand_id() {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
    memset((char *)uuid_str, '\0' , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));
    return name2id((char *)uuid_str, strlen(uuid_str));
}

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data) {
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;

    switch_threadattr_create(&attr, pool);
    switch_threadattr_detach_set(attr, 1);
    switch_threadattr_stacksize_set(attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, attr, fun, data, pool);

    switch_mutex_lock(globals.mutex);
    globals.active_threads++;
    switch_mutex_unlock(globals.mutex);

    return;
}

static script_t *script_lookup(char *name) {
    script_t *script = NULL;
    int32_t id = 0;

    if(zstr(name) || globals.fl_shutdown) {
        return NULL;
    }
    id = name2id(name, strlen(name));

    switch_mutex_lock(globals.mutex_scripts);
    script = switch_core_inthash_find(globals.scripts_map, id);
    switch_mutex_unlock(globals.mutex_scripts);

    return script;
}

static script_instance_t *instance_lookup(script_t *script, uint32_t id) {
    script_instance_t *instance = NULL;

    if(!script || globals.fl_shutdown) {
        return NULL;
    }
    if(script_sem_take(script)) {
        switch_mutex_lock(script->mutex);
        instance = switch_core_inthash_find(script->instances_map, id);
        switch_mutex_unlock(script->mutex);
        script_sem_release(script);
    }
    return instance;
}

static uint32_t script_sem_take(script_t *script) {
    uint32_t status = SWITCH_FALSE;

    if(!script) { return SWITCH_FALSE; }

    switch_mutex_lock(script->mutex);
    if(script->fl_ready) {
        status = SWITCH_TRUE;
        script->tx_sem++;
    }
    switch_mutex_unlock(script->mutex);
    return status;
}

static void script_sem_release(script_t *script) {
    switch_assert(script);

    switch_mutex_lock(script->mutex);
    if(script->tx_sem) {
        script->tx_sem--;
    }
    switch_mutex_unlock(script->mutex);
}

static uint32_t script_instance_sem_take(script_instance_t *instance) {
    uint32_t status = SWITCH_FALSE;

    if(!instance) { return SWITCH_FALSE; }

    switch_mutex_lock(instance->mutex);
    if(instance->fl_ready) {
        status = SWITCH_TRUE;
        instance->tx_sem++;
    }
    switch_mutex_unlock(instance->mutex);
    return status;
}

static void script_instance_sem_release(script_instance_t *instance) {
    switch_assert(instance);

    switch_mutex_lock(instance->mutex);
    if(instance->tx_sem) {
        instance->tx_sem--;
    }
    switch_mutex_unlock(instance->mutex);
}

static switch_status_t script_load_code(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_file_t *fd = NULL;

    switch_assert(script);

    if((status = switch_file_open(&fd, script->path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, script->pool)) != SWITCH_STATUS_SUCCESS) {
        return status;
    }
    script->code_length = switch_file_get_size(fd);
    if(!script->code_length) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Script file is empty: %s\n", script->path);
        status = SWITCH_STATUS_FALSE;
        goto out;
    }
    if((script->code = switch_core_alloc(script->pool, script->code_length + 1)) == NULL)  {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        status = SWITCH_STATUS_MEMERR;
        goto out;
    }
    if(switch_file_read(fd, script->code, &script->code_length) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't read file\n");
        return SWITCH_STATUS_GENERR;
    }
    script->code[script->code_length] = '\0';
out:
    switch_file_close(fd);
    return status;
}

static switch_status_t script_configure_ctx(script_t *script, script_instance_t *script_instance) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    JSContext *ctx = script_instance->ctx;
    JSValue global_obj, script_obj, argc_obj, argv_obj;

    /* init */
    JS_SetContextOpaque(ctx, script_instance);
    global_obj = JS_GetGlobalObject(ctx);

    /* built-in classes */
    js_session_class_register(ctx, global_obj);
    //js_codec_class_register(ctx, global_obj);
    //js_file_handle_class_register(ctx, global_obj);
    //js_event_class_register(ctx, global_obj);
    //js_dtmf_class_register(ctx, global_obj);
    //js_fileio_class_register(ctx, global_obj);
    //js_file_class_register(ctx, global_obj);
    //js_socket_class_register(ctx, global_obj);
    //js_coredb_class_register(ctx, global_obj);
    //js_eventhandler_class_register(ctx, global_obj);
#ifdef JS_CURL_ENABLE
    //js_curl_class_register(ctx, global_obj);
#endif // JS_CURL_ENABLE

    /* script obj */
    script_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, script_obj, "name", JS_NewString(ctx, script->name));
    JS_SetPropertyStr(ctx, script_obj, "instance_id", JS_NewInt32(ctx, script_instance->id));
    JS_SetPropertyStr(ctx, global_obj, "script", script_obj);

    /* arguments */
    if(!zstr(script_instance->args)) {
        char *argv[512];
        int argc = 0;

        argc = switch_separate_string(script_instance->args, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
        argc_obj = JS_NewInt32(ctx, argc);
        argv_obj = JS_NewArray(ctx);

        if(argc) {
            for (int i = 0; i < argc; i++) {
                JS_SetPropertyUint32(ctx, argv_obj, (uint32_t) i, JS_NewString(ctx, argv[i]));
            }
        }
        JS_SetPropertyStr(ctx, global_obj, "argc", argc_obj);
        JS_SetPropertyStr(ctx, global_obj, "argv", argv_obj);
    } else {
        JS_SetPropertyStr(ctx, global_obj, "argc", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, global_obj, "argv", JS_NewArray(ctx));
    }

    JS_SetPropertyStr(ctx, global_obj, "console_log", JS_NewCFunction(ctx, js_console_log, "console_log", 0));
    JS_SetPropertyStr(ctx, global_obj, "include", JS_NewCFunction(ctx, js_include, "include", 1));
    JS_SetPropertyStr(ctx, global_obj, "msleep", JS_NewCFunction(ctx, js_msleep, "msleep", 1));
    JS_SetPropertyStr(ctx, global_obj, "bridge", JS_NewCFunction(ctx, js_bridge, "bridge", 2));
    JS_SetPropertyStr(ctx, global_obj, "system", JS_NewCFunction(ctx, js_system, "system", 1));
    JS_SetPropertyStr(ctx, global_obj, "exit", JS_NewCFunction(ctx, js_exit, "exit", 0));
    JS_SetPropertyStr(ctx, global_obj, "apiExecute", JS_NewCFunction(ctx, js_api_execute, "apiExecute", 2));
    JS_SetPropertyStr(ctx, global_obj, "setGlobalVariable", JS_NewCFunction(ctx, js_global_set, "setGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "getGlobalVariable", JS_NewCFunction(ctx, js_global_get, "getGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "setInterruptHandler", JS_NewCFunction(ctx, js_global_set_interrupt_handler, "setInterruptHandler", 1));

out:
    JS_FreeValue(ctx, global_obj);
    return status;
}

static switch_status_t script_destroy(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if(script_sem_take(script)) {
        script->fl_ready = SWITCH_FALSE;
        script->fl_destroyed = SWITCH_TRUE;

        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_delete(globals.scripts_map, script->id);
        switch_mutex_unlock(globals.mutex_scripts);

        while(script->tx_sem > 1) {
            switch_yield(100000);
        }

        switch_core_destroy_memory_pool(&script->pool);
    }
    return status;
}

static switch_status_t script_launch(switch_core_session_t *session, char *script_name, char *script_args, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_path_local = NULL;
    char *script_args_local = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_memory_pool_t *ipool = NULL;
    script_t *script = NULL;
    script_instance_t *script_instance = NULL;
    uint32_t id = 0;

    if(zstr(script_name)) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }

    id = name2id(script_name, strlen(script_name));
    if(!zstr(script_args)) {
        script_args_local = strdup(script_args);
    }

    if(switch_file_exists(script_name, NULL) == SWITCH_STATUS_SUCCESS) {
        script_path_local = strdup(script_name);
    } else {
        script_path_local = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script_name);
        if(switch_file_exists(script_path_local, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found: %s\n", script_path_local);
            status = SWITCH_STATUS_FALSE;
            goto out;
        }
    }

    script = script_lookup(script_name);
    if(!script) {
        if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        if((script = switch_core_alloc(pool, sizeof(script_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }

        script->id = id;
        script->pool = pool;
        script->path = switch_core_strdup(pool, script_path_local);
        script->name = basename(script->path);

        switch_core_inthash_init(&script->instances_map);
        switch_mutex_init(&script->mutex, SWITCH_MUTEX_NESTED, pool);

        if((status = script_load_code(script)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load script\n");
            goto out;
        }

        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_insert(globals.scripts_map, script->id, script);
        switch_mutex_unlock(globals.mutex_scripts);

        script->fl_ready = SWITCH_TRUE;
    }

    if(script_sem_take(script)) {
        if(switch_core_new_memory_pool(&ipool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        if((script_instance = switch_core_alloc(ipool, sizeof(script_instance_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        script_instance->pool = ipool;
        script_instance->script = script;
        script_instance->session = session;
        script_instance->session_id = (session ? switch_core_session_get_uuid(session) : NULL);
        script_instance->args = (zstr(script_args_local) ? NULL : switch_core_strdup(ipool, script_args_local));
        script_instance->id = (zstr(script_instance->session_id) ? rand_id() : name2id((char *)script_instance->session_id, strlen(script_instance->session_id)));
        switch_mutex_init(&script_instance->mutex, SWITCH_MUTEX_NESTED, ipool);

        switch_mutex_lock(script->mutex);
        switch_core_inthash_insert(script->instances_map, script_instance->id, script_instance);
        script->instances++;
        switch_mutex_unlock(script->mutex);

        if(async) {
            script_sem_take(script);
            launch_thread(ipool, script_instance_thread, script_instance);
        } else {
            script_instance_thread(NULL, script_instance);
        }
        script_sem_release(script);

        if(!script->instances) {
            script_destroy(script);
        }
    } else {
        status = SWITCH_STATUS_FALSE;
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(pool)  { switch_core_destroy_memory_pool(&pool); }
        if(ipool) { switch_core_destroy_memory_pool(&ipool); }
    }
    switch_safe_free(script_path_local);
    switch_safe_free(script_args_local);
    return status;
}

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj) {
    volatile script_instance_t *_ref = (script_instance_t *) obj;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    script_instance_t *script_instance = (script_instance_t *) _ref;
    script_t *script = (script_t *)script_instance->script;
    char *script_buf_local = NULL, *script_tmp_buff = NULL;
    JSValue result;

    if(script->fl_destroyed || script_instance->fl_destroyed || globals.fl_shutdown) {
        goto out;
    }

    switch_mutex_lock(globals.mutex_rt);
    script_instance->ctx = JS_NewContext(globals.qjs_rt);
    if(script_instance->ctx) {
        status = script_configure_ctx(script, script_instance);
    }
    switch_mutex_unlock(globals.mutex_rt);

    if(!script_instance->ctx || status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create ctx\n");
        goto out;
    }

    if(script_instance->session) {
        uint32_t clen = 0;
        switch_channel_t *channel = switch_core_session_get_channel(script_instance->session);
        script_tmp_buff = switch_mprintf("var session = new Session('%s');\n", switch_channel_get_uuid(channel));

        clen = (script->code_length + strlen(script_tmp_buff));

        switch_zmalloc(script_buf_local, clen + 1);
        memcpy(script_buf_local, script_tmp_buff, strlen(script_tmp_buff));
        memcpy(script_buf_local + strlen(script_tmp_buff), script->code, script->code_length);

        script_instance->fl_ready = SWITCH_TRUE;
        result = JS_Eval(script_instance->ctx, script_buf_local, clen, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);
    } else {
        script_instance->fl_ready = SWITCH_TRUE;
        result = JS_Eval(script_instance->ctx, script->code, script->code_length, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);
    }
    if(JS_IsException(result)) {
        ctx_dump_error(script, script_instance, script_instance->ctx);
        JS_ResetUncatchableError(script_instance->ctx);
    }

    JS_FreeValue(script_instance->ctx, result);
    JS_RunGC(globals.qjs_rt);
out:
    script_instance->fl_ready = SWITCH_FALSE;
    script_instance->fl_destroyed = SWITCH_TRUE;

    switch_safe_free(script_tmp_buff);
    switch_safe_free(script_buf_local);

    switch_mutex_lock(script->mutex);
    switch_core_inthash_delete(script->instances_map, script_instance->id);
    script->instances--;
    switch_mutex_unlock(script->mutex);

    while(script_instance->tx_sem > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(script_instance->mutex);
    if(script_instance->ctx) {
        JS_FreeContext(script_instance->ctx);
    }
    switch_mutex_unlock(script_instance->mutex);

    switch_core_destroy_memory_pool(&script_instance->pool);

    script_sem_release(script);
    if(!script->instances) {
        script_destroy(script);
    }

    if(thread) {
        switch_mutex_lock(globals.mutex);
        if(globals.active_threads > 0) globals.active_threads--;
        switch_mutex_unlock(globals.mutex);
    }

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void event_handler_shutdown(switch_event_t *event) {
    globals.fl_shutdown = SWITCH_TRUE;
}

#define CMD_SYNTAX "\n" \
    "list - show running scripts\n" \
    "run scriptName [args] - launch a new script instance\n" \
    "int scriptName [id]   - interrrupt script\n"

SWITCH_STANDARD_API(quickjs_cmd) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[5] = { 0 };
    int argc = 0;

    if(!zstr(cmd)) {
        mycmd = strdup(cmd);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(argc == 0) { goto usage; }
    if(globals.fl_shutdown) { goto out; }

    if(argc == 1) {
        if(strcasecmp(argv[0], "list") == 0) {
            switch_hash_index_t *hidx = NULL;
            switch_hash_index_t *hidx2 = NULL;

            switch_mutex_lock(globals.mutex_scripts);
            for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                void *hval = NULL;
                script_t *script = NULL;

                switch_core_hash_this(hidx, NULL, NULL, &hval);
                script = (script_t *)hval;

                if(script_sem_take(script)) {
                    stream->write_function(stream, "%s [%i] (%s)\n", script->name, script->instances, script->path);
                    for(hidx2 = switch_core_hash_first_iter(script->instances_map, hidx2); hidx2; hidx2 = switch_core_hash_next(&hidx2)) {
                        script_instance_t *script_instance = NULL;
                        void *hval2 = NULL;

                        switch_core_hash_this(hidx2, NULL, NULL, &hval2);
                        script_instance = (script_instance_t *)hval2;

                        if(script_instance_sem_take(script_instance)) {
                            stream->write_function(stream, " - %X (session: %s)\n", script_instance->id, (script_instance->session_id ? script_instance->session_id : "none"));
                            script_instance_sem_release(script_instance);
                        }
                    }
                    script_sem_release(script);
                }
            }
            switch_mutex_unlock(globals.mutex_scripts);
            goto out;
        }
        goto usage;
    }
    if(strcasecmp(argv[0], "run") == 0) {
        char *script_args = (argc > 2 ? ((char *)cmd + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

        if((status = script_launch(session, argv[1], script_args, SWITCH_TRUE)) == SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "+OK\n");
        } else {
            stream->write_function(stream, "-ERR: %i\n", status);
        }
        goto out;
    }
    if(strcasecmp(argv[0], "int") == 0) {
        script_t *script = NULL;
        script_instance_t *script_instance = NULL;
        switch_hash_index_t *hidx = NULL;
        char *sname = argc > 2 ? argv[1] : NULL;
        char *sid = argc > 3 ? argv[2] : NULL;

        script = script_lookup(sname);
        if(script_sem_take(script)) {
            if(!zstr(sid)) {
                script_instance = instance_lookup(script, strtol(sid, NULL, 16));
                if(script_instance_sem_take(script_instance)) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Interrupt %s/%x [1]\n", script->name, script_instance->id);
                    //
                    //
                    script_instance_sem_release(script_instance);
                }
            } else {
                for(hidx = switch_core_hash_first_iter(script->instances_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                    void *hval = NULL;

                    switch_core_hash_this(hidx, NULL, NULL, &hval);
                    script_instance = (script_instance_t *)hval;
                    if(script_instance_sem_take(script_instance)) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Interrupt %s/%x [2]\n", script->name, script_instance->id);
                        //
                        //
                        script_instance_sem_release(script_instance);
                    }
                }
            }
            script_sem_release(script);
        }
        stream->write_function(stream, "-ERR: not yet implemented\n");
        goto out;
    }
    goto out;
usage:
    stream->write_function(stream, "-USAGE: %s\n", CMD_SYNTAX);
out:
    switch_safe_free(mycmd);
    return SWITCH_STATUS_SUCCESS;
}

#define APP_SYNTAX "scriptName [args]"
SWITCH_STANDARD_APP(quickjs_app) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[2] = { 0 };
    char *script_name = NULL, *script_args = NULL;
    int argc = 0;

    if(!zstr(data)) {
        mycmd = strdup(data);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(globals.fl_shutdown) { goto out; }
    if(argc < 1) { goto usage; }

    script_name = argv[0];
    script_args = (argc > 1 ? ((char *)data + (strlen(argv[0]) + 1)) : NULL);

    if((status = script_launch(session, script_name, script_args, SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't launch: %s\n", script_name);
    }
    goto out;
usage:
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", APP_SYNTAX);
out:
    switch_safe_free(mycmd);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
#define CONFIG_NAME "quickjs.conf"
SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg = NULL, xml = NULL, xml_settings = NULL, xml_param = NULL, xml_scripts = NULL, xml_script = NULL;
    switch_api_interface_t *cmd_interface;
    switch_application_interface_t *app_interface;

    memset(&globals, 0, sizeof (globals));
    switch_core_inthash_init(&globals.scripts_map);
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_scripts, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_rt, SWITCH_MUTEX_NESTED, pool);

    /* quickjs */
    globals.qjs_rt = JS_NewRuntime();
    if(!globals.qjs_rt) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create qjs runtime (JS_NewRuntime fail)\n");
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    JS_SetCanBlock(globals.qjs_rt, 1);
    JS_SetRuntimeInfo(globals.qjs_rt, "mod_quickjs");

    /* xml config */
    if((xml = switch_xml_open_cfg(CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open: %s\n", CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    if((xml_settings = switch_xml_child(cfg, "settings"))) {
        for(xml_param = switch_xml_child(xml_settings, "param"); xml_param; xml_param = xml_param->next) {
            char *var = (char *) switch_xml_attr_soft(xml_param, "name");
            char *val = (char *) switch_xml_attr_soft(xml_param, "value");
        }
    }
    if((xml_scripts = switch_xml_child(cfg, "autoload-scripts"))) {
        for(xml_script = switch_xml_child(xml_scripts, "script"); xml_script; xml_script = xml_script->next) {
            char *path = (char *) switch_xml_attr_soft(xml_script, "path");
            char *args = (char *) switch_xml_attr_soft(xml_script, "args");

            if(!zstr(path)) {
                if(script_launch(NULL, path, args, SWITCH_TRUE) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to launch script: %s", path);
                }
            }
        }
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(cmd_interface, "qjs", "manage quickjs scripts", quickjs_cmd, CMD_SYNTAX);
    SWITCH_ADD_APP(app_interface, "qjs", "manage quickjs scripts", "manage quickjs scripts", quickjs_app, APP_SYNTAX, SAF_NONE);

    if (switch_event_bind(modname, SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, event_handler_shutdown, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    globals.fl_shutdown = SWITCH_FALSE;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "quckjs (%s)\n", MOD_VERSION);

done:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown) {
    switch_hash_index_t *hidx = NULL;
    void *hval = NULL;

    switch_event_unbind_callback(event_handler_shutdown);

    globals.fl_shutdown = SWITCH_TRUE;
    while(globals.active_threads > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(globals.mutex_scripts);
    for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
        script_t *script = NULL;

        switch_core_hash_this(hidx, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            //
            // todo: try to interrupt
            //
            script_sem_release(script);
        }
    }
    switch_safe_free(hidx);
    switch_core_inthash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts);

    if(globals.qjs_rt) {
        JS_FreeRuntime(globals.qjs_rt);
    }

    return SWITCH_STATUS_SUCCESS;
}



