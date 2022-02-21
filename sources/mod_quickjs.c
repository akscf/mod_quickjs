/**
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "mod_quickjs.h"

static struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts_map;
    switch_hash_t           *scripts_map;
    size_t                  cfg_rt_mem_limit;
    size_t                  cfg_rt_stk_size;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    uint32_t                active_threads;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

static void *SWITCH_THREAD_FUNC script_thread(switch_thread_t *thread, void *obj);
static void *SWITCH_THREAD_FUNC js_spawn_thread(switch_thread_t *thread, void *obj);

static script_t *script_lookup(char *id);
static uint32_t script_sem_take(script_t *script);
static void script_sem_release(script_t *script);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ctx_dump_error(script_t *script, JSContext *ctx) {
    JSValue exception_val = JS_GetException(ctx);

    if(JS_IsError(ctx, exception_val)) {
        JSValue stk_val = JS_GetPropertyStr(ctx, exception_val, "stack");
        const char *err_str = JS_ToCString(ctx, exception_val);
        const char *stk_str = (JS_IsUndefined(stk_val) ? NULL : JS_ToCString(ctx, stk_val));

        if(script) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "(%s:%s)\n%s %s\n", script->id, script->name, (err_str ? err_str : "[exception]"), stk_str);
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
        return JS_EXCEPTION;
    }

    exit_code = JS_ToCString(ctx, argv[0]);
    if(zstr(exit_code)) {
        return JS_EXCEPTION;
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
    script_t *script = NULL;
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

    script = JS_GetContextOpaque(ctx);
    if(!script) {
        return JS_ThrowTypeError(ctx, "Invalid ctx");
    }

    SWITCH_STANDARD_STREAM(stream);
    switch_api_execute(api_str, arg_str, script->session, &stream);
    js_ret_val = JS_NewString(ctx, switch_str_nil((char *) stream.data));

    switch_safe_free(stream.data);
    JS_FreeCString(ctx, api_str);
    JS_FreeCString(ctx, arg_str);

    return js_ret_val;
}

static JSValue js_md5(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
    const char *str = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    str = JS_ToCString(ctx, argv[0]);
    if(zstr(str)) {
        return JS_UNDEFINED;
    }

    switch_md5_string(digest, (void *) str, strlen(str));
    JS_FreeCString(ctx, str);

    return JS_NewStringLen(ctx, digest, sizeof(digest));
}

static JSValue js_include(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ret_val = JS_FALSE;
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

    script = JS_GetContextOpaque(ctx);
    if(!script) { return JS_ThrowTypeError(ctx, "Invalid ctx"); }

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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
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
            ctx_dump_error(script, ctx);
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

static JSValue js_is_interrupted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);

    if(!script) {
        return JS_FALSE;
    }
    return (script->fl_interrupted ? JS_TRUE : JS_FALSE);
}

static JSValue js_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return js_session_ext_bridge(ctx, this_val, argc, argv);
}

static JSValue js_spawn(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = NULL;
    switch_memory_pool_t *pool = NULL;
    js_thread_t *js_thread = NULL;
    JSValue obj = JS_UNDEFINED;

    script = JS_GetContextOpaque(ctx);
    if(!script) { return JS_ThrowTypeError(ctx, "Invalid ctx"); }

    if(argc < 1) {
        return JS_FALSE;
    }

    if(JS_IsFunction(ctx, argv[0])) {
        if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
            goto fail;
        }
        if((js_thread = switch_core_alloc(pool, sizeof(js_thread_t))) == NULL) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            goto fail;
        }

        js_thread->ctx = JS_NewContext(JS_GetRuntime(ctx));
        if(!js_thread->ctx) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't create ctx\n");
            goto fail;
        }

        switch_mutex_lock(script->mutex);
        js_thread->id = script->thread_last_id++;
        switch_mutex_unlock(script->mutex);

        switch_mutex_init(&js_thread->mutex, SWITCH_MUTEX_NESTED, pool);

        js_thread->refs = 0;
        js_thread->pool = pool;
        js_thread->script = script;
        js_thread->arg = (argc > 1 ? JS_DupValue(ctx, argv[1]) : JS_UNDEFINED);
        js_thread->fn = JS_DupValue(ctx, argv[0]);

        obj = js_thread_object_create(ctx, js_thread);
        if(JS_IsException(obj)) { goto fail; }
        js_thread->tobj = JS_DupValue(ctx, obj);

        switch_mutex_lock(script->mutex_threads_map);
        switch_core_inthash_insert(script->threads_map, js_thread->id, js_thread);
        switch_mutex_unlock(script->mutex_threads_map);

        switch_mutex_lock(script->mutex);
        script->threads_active++;
        switch_mutex_unlock(script->mutex);

        js_thread->fl_ready = SWITCH_TRUE;
        launch_thread(pool, js_spawn_thread, js_thread);

        return obj;
    }
fail:
    JS_FreeValue(ctx, obj);

    if(js_thread->ctx) {
        JS_FreeContext(js_thread->ctx);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    return JS_FALSE;
}

switch_status_t js_register_classid(JSRuntime *rt, const char *class_name, JSClassID class_id) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    script_t *script = NULL;
    JSClassID *ptr = NULL;

    switch_assert(rt);

    script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);

    if(script_sem_take(script)) {
        if(!(ptr = switch_core_alloc(script->pool, sizeof(JSClassID)))) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            script_sem_release(script);
            goto out;
        }
        memcpy(ptr, &class_id, sizeof(JSClassID));

        switch_mutex_lock(script->mutex_classes_map);
        switch_core_hash_insert(script->classes_map, class_name, ptr);
        switch_mutex_unlock(script->mutex_classes_map);

        script_sem_release(script);
        status = SWITCH_STATUS_SUCCESS;
    }
out:
    return status;
}

JSClassID js_lookup_classid(JSRuntime *rt, const char *class_name) {
    script_t *script = NULL;
    JSClassID *ptr = NULL, id = 0;

    switch_assert(rt);

    script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);

    if(script_sem_take(script)) {
        switch_mutex_lock(script->mutex_classes_map);
        ptr = switch_core_hash_find(script->classes_map, class_name);
        if(ptr) {
            id = (JSClassID) *ptr;
        }
        switch_mutex_unlock(script->mutex_classes_map);
        script_sem_release(script);
    }

    return id;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
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

/* lookup and take sem */
js_thread_t *js_thread_lookup(JSContext *ctx, uint32_t id) {
    js_thread_t *js_thread = NULL;
    script_t *script = NULL;
    uint8_t success = 0;

    script = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));
    switch_assert(script);

    if(script_sem_take(script)) {
        switch_mutex_lock(script->mutex_threads_map);
        js_thread = switch_core_inthash_find(script->threads_map, id);
        if(js_thread && js_thread->fl_ready) {
            switch_mutex_lock(js_thread->mutex);
            js_thread->refs++;
            switch_mutex_unlock(js_thread->mutex);
            success = 1;
        }
        switch_mutex_unlock(script->mutex_threads_map);
        script_sem_release(script);
    }
    return (success ? js_thread : NULL);
}

int js_thread_sem_take(js_thread_t *js_thread) {
    if(js_thread && js_thread->fl_ready) {
        switch_mutex_lock(js_thread->mutex);
        js_thread->refs++;
        switch_mutex_unlock(js_thread->mutex);
        return 1;
    }
    return 0;
}

void js_thread_sem_release(js_thread_t *js_thread) {
    if(js_thread && js_thread->fl_ready) {
        switch_mutex_lock(js_thread->mutex);
        if(js_thread->refs > 0) js_thread->refs--;
        switch_mutex_unlock(js_thread->mutex);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool) {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

    memset((char *)uuid_str, '\0' , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));

    *uuid = switch_core_strdup(pool, uuid_str);

    return SWITCH_STATUS_SUCCESS;
}

static script_t *script_lookup(char *id) {
    script_t *script = NULL;

    if(zstr(id)) {
        return NULL;
    }

    switch_mutex_lock(globals.mutex_scripts_map);
    script = switch_core_hash_find(globals.scripts_map, id);
    switch_mutex_unlock(globals.mutex_scripts_map);

    return script;
}

static uint32_t script_sem_take(script_t *script) {
    uint32_t status = SWITCH_FALSE;

    if(!script) { return SWITCH_FALSE; }

    switch_mutex_lock(script->mutex);
    if(script->fl_ready) {
        status = SWITCH_TRUE;
        script->sem++;
    }
    switch_mutex_unlock(script->mutex);
    return status;
}

static void script_sem_release(script_t *script) {
    switch_assert(script);

    switch_mutex_lock(script->mutex);
    if(script->sem) {
        script->sem--;
    }
    switch_mutex_unlock(script->mutex);
}

static switch_status_t script_load(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_file_t *fd = NULL;

    if(!script) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }

    if((status = switch_file_open(&fd, script->path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, script->pool)) != SWITCH_STATUS_SUCCESS) {
        return status;
    }

    script->script_len = switch_file_get_size(fd);
    if(!script->script_len) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Script file is empty: %s\n", script->path);
        status = SWITCH_STATUS_FALSE;
        goto out;
    }

    if((script->script_buf = switch_core_alloc(script->pool, script->script_len + 1)) == NULL)  {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        status = SWITCH_STATUS_MEMERR;
        goto out;
    }

    if(switch_file_read(fd, script->script_buf, &script->script_len) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't read file\n");
        return SWITCH_STATUS_GENERR;
    }

    script->script_buf[script->script_len] = '\0';
out:
    if(fd) {
        switch_file_close(fd);
    }
    return status;
}

static switch_status_t script_launch(switch_core_session_t *session, char *script_name, char *script_args, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_path_local = NULL;
    char *script_args_local = NULL;
    switch_memory_pool_t *pool = NULL;
    script_t *script = NULL;

    if(zstr(script_name)) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }
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

    if(session) {
        script->id = switch_core_strdup(pool, switch_core_session_get_uuid(session));
    } else {
        new_uuid(&script->id, pool);
    }

    script->pool = pool;
    script->path = switch_core_strdup(pool, script_path_local);
    script->name = basename(script->path);
    script->args = (!zstr(script_args_local) ? switch_core_strdup(pool, script_args_local) : NULL);
    script->session_id = (session ? switch_core_session_get_uuid(session) : NULL);
    script->session = session;

    switch_core_hash_init(&script->classes_map);
    switch_core_inthash_init(&script->threads_map);
    switch_mutex_init(&script->mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&script->mutex_classes_map, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&script->mutex_threads_map, SWITCH_MUTEX_NESTED, pool);

    status = script_load(script);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load script\n");
        goto out;
    }

    switch_mutex_lock(globals.mutex_scripts_map);
    switch_core_hash_insert(globals.scripts_map, script->id, script);
    switch_mutex_unlock(globals.mutex_scripts_map);

    if(async) {
        launch_thread(pool, script_thread, script);
    } else {
        script_thread(NULL, script);
    }

out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(pool)  {
            switch_core_destroy_memory_pool(&pool);
        }
    }
    switch_safe_free(script_path_local);
    switch_safe_free(script_args_local);
    return status;
}

static void *SWITCH_THREAD_FUNC js_spawn_thread(switch_thread_t *thread, void *obj) {
    volatile js_thread_t *_ref = (js_thread_t *) obj;
    js_thread_t *js_thread = (js_thread_t *) _ref;
    script_t *script = js_thread->script;
    switch_memory_pool_t *pool = js_thread->pool;
    JSContext *ctx = js_thread->ctx;
    JSValue fn = js_thread->fn;
    uint32_t thread_id = js_thread->id;
    uint32_t dlock_cnt = 0;
    JSValue result;
    JSValue args[2];

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "thread [%u] stated\n", thread_id);

    JS_SetContextOpaque(ctx, script);

    args[0] = js_thread->tobj;   // thread info
    args[1] = js_thread->arg;    // spawn args

    result = JS_Call(ctx, fn, JS_UNDEFINED, 2, (JSValueConst *)args);
    if(JS_IsException(result)) {
        ctx_dump_error(script, ctx);
        JS_ResetUncatchableError(ctx);
    }
out:
    JS_FreeValue(ctx, result);
    JS_FreeValue(ctx, fn);
    JS_FreeValue(ctx, args[0]);
    JS_FreeValue(ctx, args[1]);

    js_thread->fl_ready = SWITCH_FALSE;
    js_thread->fl_destroyed = SWITCH_TRUE;

    if(js_thread->refs) {
        switch_yield(100000);
        dlock_cnt++;
        if(dlock_cnt > 100) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "thread [%s/%u] has '%u' active references!\n", script->name, js_thread->id, js_thread->refs);
            dlock_cnt = 0;
        }
    }

    if(script) {
        switch_mutex_lock(script->mutex_threads_map);
        switch_core_inthash_delete(script->threads_map, thread_id);
        switch_mutex_unlock(script->mutex_threads_map);

        switch_mutex_lock(script->mutex);
        if(script->threads_active > 0) script->threads_active--;
        switch_mutex_unlock(script->mutex);
    }

    if(ctx) {
        JS_FreeContext(ctx);
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "thread [%u] finished\n", thread_id);

    switch_mutex_lock(globals.mutex);
    if(globals.active_threads > 0) globals.active_threads--;
    switch_mutex_unlock(globals.mutex);

    return NULL;
}

static void *SWITCH_THREAD_FUNC script_thread(switch_thread_t *thread, void *obj) {
    volatile script_t *_ref = (script_t *) obj;
    script_t *script = (script_t *) _ref;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_buf_local = NULL;
    char *script_tmp_buff = NULL;
    uint8_t fl_curl_enable=0, fl_odbc_enable=0;
    uint32_t threads_dlock_cnt = 0;
    switch_hash_index_t *hidx = NULL;
    JSContext *ctx = NULL;
    JSRuntime *rt = NULL;
    JSValue global_obj, script_obj, argc_obj, argv_obj, flags_obj;
    JSValue result;

    if(script->fl_destroyed || globals.fl_shutdown) {
        goto out;
    }

    if(!(rt = JS_NewRuntime())) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create jsRuntime\n");
        goto out;
    }
    if(!(ctx = JS_NewContext(rt))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create jsCtx\n");
        goto out;
    }

    if(globals.cfg_rt_mem_limit) {
        JS_SetMemoryLimit(rt, globals.cfg_rt_mem_limit);
    }
    if(globals.cfg_rt_stk_size) {
        JS_SetMaxStackSize(rt, globals.cfg_rt_stk_size);
    }

    script->rt = rt;
    script->ctx = ctx;

    JS_SetCanBlock(rt, 1);
    JS_SetRuntimeInfo(rt, script->name);
    JS_SetRuntimeOpaque(rt, script);
    JS_SetContextOpaque(ctx, script);

    global_obj = JS_GetGlobalObject(ctx);

    script->fl_ready = SWITCH_TRUE; /* temporary */
    js_session_class_register(ctx, global_obj);
    js_codec_class_register(ctx, global_obj);
    js_file_handle_class_register(ctx, global_obj);
    js_event_class_register(ctx, global_obj);
    js_dtmf_class_register(ctx, global_obj);
    js_file_class_register(ctx, global_obj);
    js_socket_class_register(ctx, global_obj);
    js_coredb_class_register(ctx, global_obj);
    js_eventhandler_class_register(ctx, global_obj);
    js_xml_class_register(ctx, global_obj);
    js_thread_class_register(ctx, global_obj);
#ifdef FS_MOD_ENABLE_CURL
    js_curl_class_register(ctx, global_obj);
    fl_curl_enable = 1;
#endif
#ifdef FS_MOD_ENABLE_ODBC
    js_odbc_class_register(ctx, global_obj);
    fl_odbc_enable = 1;
#endif
    script->fl_ready = SWITCH_FALSE; /* unset */

    flags_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, flags_obj, "curlEnabled", JS_NewString(ctx, (fl_curl_enable ? "true" : "false")));
    JS_SetPropertyStr(ctx, flags_obj, "odbcEnabled", JS_NewString(ctx, (fl_odbc_enable ? "true" : "false")));
    JS_SetPropertyStr(ctx, global_obj, "flags", flags_obj);

    script_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, script_obj, "id",   JS_NewString(ctx, script->id));
    JS_SetPropertyStr(ctx, script_obj, "name", JS_NewString(ctx, script->name));
    JS_SetPropertyStr(ctx, script_obj, "path", JS_NewString(ctx, script->path));
    JS_SetPropertyStr(ctx, script_obj, "isInterrupted", JS_NewCFunction(ctx, js_is_interrupted, "isInterrupted", 0));
    JS_SetPropertyStr(ctx, global_obj, "script", script_obj);

    /* arguments */
    if(!zstr(script->args)) {
        char *argv[512] = { 0 };
        int argc = 0;

        argc = switch_separate_string(script->args, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
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
    JS_SetPropertyStr(ctx, global_obj, "consoleLog", JS_NewCFunction(ctx, js_console_log, "consoleLog", 0));
    JS_SetPropertyStr(ctx, global_obj, "include", JS_NewCFunction(ctx, js_include, "include", 1));
    JS_SetPropertyStr(ctx, global_obj, "msleep", JS_NewCFunction(ctx, js_msleep, "msleep", 1));
    JS_SetPropertyStr(ctx, global_obj, "bridge", JS_NewCFunction(ctx, js_bridge, "bridge", 2));
    JS_SetPropertyStr(ctx, global_obj, "system", JS_NewCFunction(ctx, js_system, "system", 1));
    JS_SetPropertyStr(ctx, global_obj, "exit", JS_NewCFunction(ctx, js_exit, "exit", 1));
    JS_SetPropertyStr(ctx, global_obj, "md5", JS_NewCFunction(ctx, js_md5, "md5", 1));
    JS_SetPropertyStr(ctx, global_obj, "apiExecute", JS_NewCFunction(ctx, js_api_execute, "apiExecute", 2));
    JS_SetPropertyStr(ctx, global_obj, "setGlobalVariable", JS_NewCFunction(ctx, js_global_set, "setGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "getGlobalVariable", JS_NewCFunction(ctx, js_global_get, "getGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "spawn", JS_NewCFunction(ctx, js_spawn, "spawn", 1));

    if(script->session) {
        uint32_t clen = 0;
        switch_channel_t *channel = switch_core_session_get_channel(script->session);
        script_tmp_buff = switch_mprintf("var session = new Session('%s');\n", switch_channel_get_uuid(channel));

        clen = (script->script_len + strlen(script_tmp_buff));

        switch_zmalloc(script_buf_local, clen + 1); // for '\0' at the end
        memcpy(script_buf_local, script_tmp_buff, strlen(script_tmp_buff));
        memcpy(script_buf_local + strlen(script_tmp_buff), script->script_buf, script->script_len);

        script->fl_ready = SWITCH_TRUE;
        result = JS_Eval(ctx, script_buf_local, clen, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);
    } else {
        script->fl_ready = SWITCH_TRUE;
        result = JS_Eval(ctx, script->script_buf, script->script_len, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);
    }

    if(JS_IsException(result)) {
        ctx_dump_error(script, ctx);
        JS_ResetUncatchableError(ctx);
    }

    JS_FreeValue(ctx, result);

out:
    JS_FreeValue(ctx, global_obj);

    script->fl_ready = SWITCH_FALSE;
    script->fl_destroyed = SWITCH_TRUE;

    switch_safe_free(script_tmp_buff);
    switch_safe_free(script_buf_local);

    while(script->sem > 0) {
        switch_yield(100000);
    }

    if(ctx) {
        JS_FreeContext(ctx);
    }
    if(rt) {
        JS_FreeRuntime(rt);
    }
    script->rt = NULL;
    script->ctx = NULL;

    switch_mutex_lock(globals.mutex_scripts_map);
    switch_core_hash_delete(globals.scripts_map, script->id);
    switch_mutex_unlock(globals.mutex_scripts_map);

    // stop threads
    if(script->threads_active) {
        switch_mutex_lock(script->mutex_threads_map);
        for(hidx = switch_core_hash_first_iter(script->threads_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
            js_thread_t *js_thread = NULL; void *hval = NULL;

            switch_core_hash_this(hidx, NULL, NULL, &hval);
            js_thread = (js_thread_t *) hval;

            if(js_thread && !js_thread->fl_destroyed) {
                js_thread->fl_interrupted = SWITCH_TRUE;
            }
        }
        switch_mutex_unlock(script->mutex_threads_map);
    }

    while(script->threads_active) {
        switch_yield(100000);
        threads_dlock_cnt++;
        if(threads_dlock_cnt > 100) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "%s: Still active '%i' threads!\n", script->name, script->threads_active);
            threads_dlock_cnt = 0;
        }
    }

    switch_core_inthash_destroy(&script->threads_map);
    switch_core_hash_destroy(&script->classes_map);
    switch_core_destroy_memory_pool(&script->pool);

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
    "run   scriptName [args] - launch the script instance\n" \
    "int   scriptId - interrupt script\n" \
    "int   scriptId threadId - interrupt thread\n"

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

            switch_mutex_lock(globals.mutex_scripts_map);
            for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                script_t *script = NULL;
                void *hval = NULL;

                switch_core_hash_this(hidx, NULL, NULL, &hval);
                script = (script_t *)hval;

                if(script_sem_take(script)) {
                    stream->write_function(stream, "%s (%s) [threads: %u, session: %s]\n",
                            script->id, script->path, script->threads_active,
                            (script->session_id ? script->session_id : "none")
                    );
                    script_sem_release(script);
                }
            }
            switch_mutex_unlock(globals.mutex_scripts_map);
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
        js_thread_t *js_thread = NULL;
        char *sid = (argc > 1 ? argv[1] : NULL);
        char *tid = (argc > 2 ? argv[2] : NULL);
        int success = 0;

        script = script_lookup(sid);
        if(script_sem_take(script)) {
            if(script->fl_ready && !script->fl_destroyed) {
                if(!tid) {
                    script->fl_interrupted = SWITCH_TRUE;
                    success++;
                } else {
                    js_thread = js_thread_lookup(script->ctx, atol(tid));
                    if(js_thread) {
                        js_thread->fl_interrupted = SWITCH_TRUE;
                        js_thread_sem_release(js_thread);
                        success++;
                    }
                }
            }
            script_sem_release(script);
        }
        stream->write_function(stream, (success ? "+OK\n" : "-ERR: not found\n") );
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
    switch_core_hash_init(&globals.scripts_map);
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_scripts_map, SWITCH_MUTEX_NESTED, pool);

    globals.cfg_rt_mem_limit = 0;
    globals.cfg_rt_mem_limit = 0;

    /* xml config */
    if((xml = switch_xml_open_cfg(CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open: %s\n", CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    if((xml_settings = switch_xml_child(cfg, "settings"))) {
        for(xml_param = switch_xml_child(xml_settings, "param"); xml_param; xml_param = xml_param->next) {
            char *var = (char *) switch_xml_attr_soft(xml_param, "name");
            char *val = (char *) switch_xml_attr_soft(xml_param, "value");

            if(!strcasecmp(var, "rt-stack-size-max")) {
                size_t x = atoi(val);
                if(x > 0) { globals.cfg_rt_stk_size = x * 1024 * 1024; }
            }
            if(!strcasecmp(var, "rt-memory-limit")) {
                size_t x = atoi(val);
                if(x > 0) { globals.cfg_rt_mem_limit = x * 1024 * 1024; }
            }
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
    switch_hash_index_t *hi = NULL;
    script_t *script = NULL;
    void *hval = NULL;

    switch_event_unbind_callback(event_handler_shutdown);

    globals.fl_shutdown = SWITCH_TRUE;
    while(globals.active_threads > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(globals.mutex_scripts_map);
    for(hi = switch_core_hash_first_iter(globals.scripts_map, hi); hi; hi = switch_core_hash_next(&hi)) {
        switch_core_hash_this(hi, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            script->fl_interrupted = SWITCH_TRUE;
            script_sem_release(script);
        }
    }
    switch_safe_free(hi);
    switch_core_hash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts_map);

    return SWITCH_STATUS_SUCCESS;
}



