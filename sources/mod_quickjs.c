/**
 * (C)2021-2025 aks
 * https://github.com/akscf/
 **/
#include "mod_quickjs.h"
#include "js_xml.h"
#include "js_coredb.h"
#include "js_codec.h"
#include "js_file.h"
#include "js_socket.h"
#include "js_event.h"
#include "js_coredb.h"
#include "js_filehandle.h"
#include "js_eventhandler.h"
#include "js_session.h"
#include "js_curl.h"
#include "js_dbh.h"

globals_t globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

static void *SWITCH_THREAD_FUNC script_thread(switch_thread_t *thread, void *obj);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    const char *file = __FILE__;
    int line = __LINE__;

    if(argc > 1) {
        const char *lvl_str, *msg_str;

        lvl_str = (QJS_IS_NULL(argv[0]) ? NULL : JS_ToCString(ctx, argv[0]));
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

// mssleep(delayMsec)
static JSValue js_msleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t msec = 0;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    JS_ToUint32(ctx, &msec, argv[0]);

    if(msec) {
        switch_yield(msec * 1000);
    }

    return JS_TRUE;
}

// setValiable(name, value)
static JSValue js_global_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *var_str = NULL;
    const char *val_str = NULL;

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: varName");
    }

    var_str = JS_ToCString(ctx, argv[0]);
    val_str = (QJS_IS_NULL(argv[0]) ? NULL : JS_ToCString(ctx, argv[1]));

    switch_core_set_variable(var_str, val_str);

    JS_FreeCString(ctx, var_str);
    JS_FreeCString(ctx, val_str);

    return JS_TRUE;
}

// getValiable(name)
static JSValue js_global_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *var_str;
    char *val = NULL;

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: varName");
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
    script_t *script = JS_GetContextOpaque(ctx);
    JSValue ret_val;
    const char *exit_code;

    if(!argc || QJS_IS_NULL(argv[0])) {
        if(script) {
            script->fl_exit = true;
        }
        return JS_EXCEPTION;
    }

    exit_code = JS_ToCString(ctx, argv[0]);
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

    if(QJS_IS_NULL(argv[0])) {
        return JS_UNDEFINED;
    }

    cmd = JS_ToCString(ctx, argv[0]);
    result = switch_system(cmd, true);
    JS_FreeCString(ctx, cmd);

    return JS_NewInt32(ctx, result);
}

static JSValue js_api_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *api_str;
    const char *arg_str;
    script_t *script = NULL;
    switch_stream_handle_t stream = { 0 };
    JSValue js_ret_val;

    script = JS_GetContextOpaque(ctx);
    if(!script) {
        return JS_ThrowTypeError(ctx, "Malformed ctx");
    }

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: api");
    }

    api_str = JS_ToCString(ctx, argv[0]);
    arg_str = (argc > 1 && !QJS_IS_NULL(argv[1]) ? JS_ToCString(ctx, argv[1]) : NULL);

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

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_UNDEFINED;
    }

    str = JS_ToCString(ctx, argv[0]);
    switch_md5_string(digest, (void *) str, strlen(str));
    JS_FreeCString(ctx, str);

    return JS_NewStringLen(ctx, digest, sizeof(digest));
}

static JSValue js_crc32(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *str = NULL;
    uint32_t crc = 0;

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_NewInt32(ctx, 0);
    }

    str = JS_ToCString(ctx, argv[0]);
    crc = switch_crc32_8bytes((char *)str, strlen(str));
    JS_FreeCString(ctx, str);

    return JS_NewInt32(ctx, crc);
}

/* seconds */
static JSValue js_epoch_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    time_t tm = switch_epoch_time_now(NULL);
    return JS_NewInt64(ctx, tm);
}

/* microseconds */
static JSValue js_micro_time(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    time_t tm = switch_mono_micro_time_now(); //switch_micro_time_now();
    return JS_NewInt64(ctx, tm);
}

static JSValue js_is_interrupted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);

    if(!script) {
        return JS_FALSE;
    }
    return (globals.fl_shutdown || script->fl_interrupt ? JS_TRUE : JS_FALSE);
}

static JSValue js_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    return js_session_ext_bridge(ctx, this_val, argc, argv);
}

static JSValue js_unlink(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    const char *fpath = NULL;

    if(!script) {
        return JS_ThrowTypeError(ctx, "Invalid ctx");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "unlink(filename)");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_FALSE;
    }

    fpath = JS_ToCString(ctx, argv[0]);
    unlink(fpath);

    JS_FreeCString(ctx, fpath);
    return JS_TRUE;
}

static JSValue js_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    const char *fpath = NULL;
    JSValue ret_val = JS_FALSE;

    if(!script) {
        return JS_ThrowTypeError(ctx, "Invalid ctx");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "mkdir(dirName)");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_FALSE;
    }

    fpath = JS_ToCString(ctx, argv[0]);
    if(switch_directory_exists(fpath, NULL) == SWITCH_STATUS_SUCCESS) {
        ret_val = JS_TRUE;
    } else {
        if(switch_dir_make_recursive(fpath, SWITCH_DEFAULT_DIR_PERMS, NULL) == SWITCH_STATUS_SUCCESS) {
            ret_val = JS_TRUE;
        }
    }

    JS_FreeCString(ctx, fpath);
    return ret_val;
}

static JSValue js_file_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    const char *fpath = NULL;
    JSValue ret_val = JS_FALSE;

    if(!script) {
        return JS_ThrowTypeError(ctx, "Invalid ctx");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "fileExists(fileName)");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_FALSE;
    }

    fpath = JS_ToCString(ctx, argv[0]);
    if(switch_file_exists(fpath, NULL) == SWITCH_STATUS_SUCCESS) {
        ret_val = JS_TRUE;
    }

    JS_FreeCString(ctx, fpath);
    return ret_val;
}

static JSValue js_dir_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    const char *fpath = NULL;
    JSValue ret_val = JS_FALSE;

    if(!script) {
        return JS_ThrowTypeError(ctx, "Invalid ctx");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "dirExists(dirName)");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_FALSE;
    }

    fpath = JS_ToCString(ctx, argv[0]);
    if(switch_directory_exists(fpath, NULL) == SWITCH_STATUS_SUCCESS) {
        ret_val = JS_TRUE;
    }

    JS_FreeCString(ctx, fpath);
    return ret_val;
}

static JSValue js_get_path(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    const char *type = NULL;
    JSValue ret_val = JS_UNDEFINED;

    if(!script) {
        return JS_UNDEFINED;
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "getPath(tmp|cache|scripts|sounds)");
    }

    type = JS_ToCString(ctx, argv[0]);

    if(!strcasecmp(type, "tmp")) {
        ret_val = JS_NewString(ctx, SWITCH_GLOBAL_dirs.temp_dir);
    } else if(!strcasecmp(type, "cache")) {
        ret_val = JS_NewString(ctx, SWITCH_GLOBAL_dirs.cache_dir);
    } else if(!strcasecmp(type, "scripts")) {
        ret_val = JS_NewString(ctx, SWITCH_GLOBAL_dirs.script_dir);
    } else if(!strcasecmp(type, "sounds")) {
        ret_val = JS_NewString(ctx, SWITCH_GLOBAL_dirs.sounds_dir);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unsupported type (%s)\n", type);
    }

    JS_FreeCString(ctx, type);

    return ret_val;
}

static JSValue js_get_uuid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    char tmp_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };

    if(!script) {
        return JS_UNDEFINED;
    }

    switch_uuid_str((char *)tmp_uuid, sizeof(tmp_uuid));
    return JS_NewString(ctx, (char *)tmp_uuid);
}

// chatSend(protocol, from, to, message)
static JSValue js_chat_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    script_t *script = JS_GetContextOpaque(ctx);
    JSValue ret = JS_FALSE;
    const char *proto = NULL;
    const char *from = NULL;
    const char *to = NULL;
    const char *msg = NULL;

    if(!script) {
        return JS_UNDEFINED;
    }

    if(argc < 4 || QJS_IS_NULL(argv[0]) || QJS_IS_NULL(argv[1]) || QJS_IS_NULL(argv[2]) || QJS_IS_NULL(argv[3])) {
        return JS_ThrowTypeError(ctx, "chatSend(protocol, from, to, message)");
    }

    proto = JS_ToCString(ctx, argv[0]);
    from = JS_ToCString(ctx, argv[1]);
    to = JS_ToCString(ctx, argv[2]);
    msg = JS_ToCString(ctx, argv[3]);

    if(switch_core_chat_send_args(proto, "global", from, to, "", msg, NULL, "", SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
        ret = JS_TRUE;
    }

    JS_FreeCString(ctx, proto);
    JS_FreeCString(ctx, from);
    JS_FreeCString(ctx, to);
    JS_FreeCString(ctx, msg);

    return ret;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void xx_js_module_destructor(void *data) {
    if(data) {
        dlclose(data);
    }
}

static JSModuleDef *xxx_js_module_loader(JSContext *ctx, const char *module_name) {
    JSModuleDef *m = NULL;
    JSValue func_val;
    size_t buf_len;
    uint8_t *buf;
    char *filename;

    if(!strchr(module_name, '/')) {
        filename = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, module_name);
    } else {
        filename = (char *)module_name;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "load module [%s] (%s)\n", module_name, filename);
#endif

    buf = js_load_file(ctx, &buf_len, filename);
    if(filename != module_name) {
        switch_safe_free(filename);
    }

    if(!buf) {
        JS_ThrowReferenceError(ctx, "Unable to load module '%s'", module_name);
        return NULL;
    }

    func_val = JS_Eval(ctx, (char *)buf, buf_len, module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    js_free(ctx, buf);

    if(JS_IsException(func_val)) {
        return NULL;
    }

    js_module_set_import_meta(ctx, func_val, TRUE, FALSE);
    m = JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);

    return m;

}
static JSModuleDef *xxx_so_module_loader(JSContext *ctx, const char *module_name) {
    script_t *script = JS_GetContextOpaque(ctx);
    JSModuleDef *m = NULL;
    JSInitModuleFunc *init = NULL;
    char *filename = NULL;
    void *hd = NULL;

    if(!strchr(module_name, '/')) {
        filename = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.lib_dir, SWITCH_PATH_SEPARATOR, module_name);
    } else {
        filename = (char *)module_name;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "load module [%s] (%s)\n", module_name, filename);
#endif

    hd = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
    if(filename != module_name) {
        switch_safe_free(filename);
    }

    if(!hd) {
        JS_ThrowReferenceError(ctx, "Unable to load module '%s' (%s)", module_name, dlerror());
        goto fail;
    }

    init = dlsym(hd, "js_init_module");
    if(!init) {
        dlclose(hd);
        JS_ThrowReferenceError(ctx, "Unable to load module '%s' (missing symbol: js_init_module)", module_name);
        goto fail;
    }

    m = init(ctx, module_name);
    if(m) {
        js_list_add(script->mod_hlist, hd, xx_js_module_destructor);
        return m;
    }

    /* fail */
    dlclose(hd);
    JS_ThrowReferenceError(ctx, "Unable to load module '%s' (initialization failed)", module_name);
fail:
    return NULL;
}

JSModuleDef *xxx_module_loader(JSContext *ctx, const char *module_name, void *opaque) {
    JSModuleDef *m = NULL;

    if (has_suffix(module_name, ".so")) {
        m = xxx_so_module_loader(ctx, module_name);
    } else {
        if(!strcasecmp(module_name, "std")) {
            m = js_init_module_std(ctx, "std");
        } else if(!strcasecmp(module_name, "os")) {
            m = js_init_module_os(ctx, "os");
        } else {
            m = xxx_js_module_loader(ctx, module_name);
        }
    }

    return m;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Script file is empty (%s)\n", script->path);
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    if((script->script_buf = switch_core_alloc(script->pool, script->script_len + 1)) == NULL)  {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_alloc()\n");
        switch_goto_status(SWITCH_STATUS_MEMERR, out);
    }

    if(switch_file_read(fd, script->script_buf, &script->script_len) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't read file\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    script->script_buf[script->script_len] = '\0';

out:
    if(fd) {
        switch_file_close(fd);
    }
    return status;
}

static switch_status_t script_launch(switch_core_session_t *session, char *script_name, char *script_args, char *script_id, uint8_t inbg) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_path_local = NULL;
    char *script_args_local = NULL;
    switch_memory_pool_t *pool = NULL;
    script_t *script = NULL;

    if(zstr(script_name)) {
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    if(!zstr(script_args)) {
        script_args_local = strdup(script_args);
    }

    if(switch_file_exists(script_name, NULL) == SWITCH_STATUS_SUCCESS) {
        script_path_local = strdup(script_name);
    } else {
        script_path_local = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script_name);
        if(switch_file_exists(script_path_local, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found (%s)\n", script_path_local);
            switch_goto_status(SWITCH_STATUS_MEMERR, out);
        }
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        status = SWITCH_STATUS_MEMERR;
        goto out;
    }
    if((script = switch_core_alloc(pool, sizeof(script_t))) == NULL)  {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_alloc()\n");
        switch_goto_status(SWITCH_STATUS_MEMERR, out);
    }
    if(js_list_create(&script->mod_hlist) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_list_create()\n");
        switch_goto_status(SWITCH_STATUS_MEMERR, out);
    }

    if(script_id) {
        script->id = switch_core_strdup(pool, script_id);
    } else {
        new_uuid(&script->id, pool);
    }

    script->pool = pool;
    script->path = switch_core_strdup(pool, script_path_local);
    script->name = basename(script->path);
    script->args = (!zstr(script_args_local) ? switch_core_strdup(pool, script_args_local) : NULL);
    script->session_id = (session ? switch_core_session_get_uuid(session) : NULL);
    script->session = session;

    switch_mutex_init(&script->mutex, SWITCH_MUTEX_NESTED, pool);

    status = script_load(script);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load script\n");
        goto out;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "script-id (%s) [%s]\n", script->id, script->name);

    switch_mutex_lock(globals.mutex_scripts_map);
    switch_core_hash_insert(globals.scripts_map, script->id, script);
    switch_mutex_unlock(globals.mutex_scripts_map);

    if(inbg) {
        launch_thread(pool, script_thread, script);
    } else {
        script_thread(NULL, script);
    }

out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(script) {
            if(script->mod_hlist) js_list_destroy(&script->mod_hlist);
        }
        if(pool)  {
            switch_core_destroy_memory_pool(&pool);
        }
    }
    switch_safe_free(script_path_local);
    switch_safe_free(script_args_local);
    return status;
}

static void *SWITCH_THREAD_FUNC script_thread(switch_thread_t *thread, void *obj) {
    volatile script_t *_ref = (script_t *) obj;
    script_t *script = (script_t *) _ref;
    switch_memory_pool_t *pool = script->pool;
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    JSContext *ctx = NULL;
    JSRuntime *rt = NULL;
    JSValue global_obj, session_obj, script_obj, runtime_obj, argc_obj, argv_obj;
    JSValue result;

    if(script->fl_destroyed || globals.fl_shutdown) {
        goto out;
    }

    if(!(rt = JS_NewRuntime())) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create runtime (jsRuntime)\n");
        goto out;
    }
    if(!(ctx = JS_NewContext(rt))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create context (jsCtx)\n");
        goto out;
    }

    if(globals.cfg_rt_mem_limit) {
        JS_SetMemoryLimit(rt, globals.cfg_rt_mem_limit);
    }
    if(globals.cfg_rt_stk_size) {
        JS_SetMaxStackSize(rt, globals.cfg_rt_stk_size);
    }

    JS_SetModuleLoaderFunc(rt, NULL, xxx_module_loader, NULL);

    script->rt = rt;
    script->ctx = ctx;

    JS_SetCanBlock(rt, 1);
    JS_SetRuntimeInfo(rt, script->name);
    JS_SetRuntimeOpaque(rt, script);
    JS_SetContextOpaque(ctx, script);

    global_obj = JS_GetGlobalObject(ctx);

    /* register classes */
    script->fl_ready = true; // temporary
    js_session_class_register(ctx, global_obj, 1000);
    js_codec_class_register(ctx, global_obj, 1001);
    js_file_handle_class_register(ctx, global_obj, 1002);
    js_event_class_register(ctx, global_obj, 1003);
    js_file_class_register(ctx, global_obj, 1004);
    js_socket_class_register(ctx, global_obj, 1005);
    js_coredb_class_register(ctx, global_obj, 1006);
    js_eventhandler_class_register(ctx, global_obj, 1007);
    js_xml_class_register(ctx, global_obj, 1008);
    js_curl_class_register(ctx, global_obj, 1009);
    js_dbh_class_register(ctx, global_obj, 10010);
    script->fl_ready = false; // clear

    runtime_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, runtime_obj, "type", JS_NewString(ctx, MOD_RT_TYPE));
    JS_SetPropertyStr(ctx, runtime_obj, "version", JS_NewString(ctx, MOD_VERSION));
    JS_SetPropertyStr(ctx, runtime_obj, "switchId", JS_NewString(ctx, switch_core_get_uuid()));
    JS_SetPropertyStr(ctx, runtime_obj, "switchName", JS_NewString(ctx, switch_core_get_switchname()));
    JS_SetPropertyStr(ctx, runtime_obj, "switchVersion", JS_NewString(ctx, switch_version_full()));
    JS_SetPropertyStr(ctx, runtime_obj, "hostname", JS_NewString(ctx, switch_core_get_hostname()));
    JS_SetPropertyStr(ctx, global_obj, "runtime", runtime_obj);

    script_obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, script_obj, "id",   JS_NewString(ctx, script->id));
    JS_SetPropertyStr(ctx, script_obj, "name", JS_NewString(ctx, script->name));
    JS_SetPropertyStr(ctx, script_obj, "path", JS_NewString(ctx, script->path));
    JS_SetPropertyStr(ctx, script_obj, "isInterrupted", JS_NewCFunction(ctx, js_is_interrupted, "isInterrupted", 0));
    JS_SetPropertyStr(ctx, global_obj, "script", script_obj);

    /* arguments */
    if(!zstr(script->args)) {
        char *argv[32] = { 0 };
        int argc = 0;

        argc = switch_separate_string(script->args, ' ', argv, ARRAY_SIZE(argv));
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

    /* global fncs */
    JS_SetPropertyStr(ctx, global_obj, "console_log", JS_NewCFunction(ctx, js_console_log, "console_log", 0));
    JS_SetPropertyStr(ctx, global_obj, "consoleLog", JS_NewCFunction(ctx, js_console_log, "consoleLog", 0));
    JS_SetPropertyStr(ctx, global_obj, "msleep", JS_NewCFunction(ctx, js_msleep, "msleep", 1));
    JS_SetPropertyStr(ctx, global_obj, "bridge", JS_NewCFunction(ctx, js_bridge, "bridge", 2));
    JS_SetPropertyStr(ctx, global_obj, "system", JS_NewCFunction(ctx, js_system, "system", 1));
    JS_SetPropertyStr(ctx, global_obj, "exit", JS_NewCFunction(ctx, js_exit, "exit", 1));
    JS_SetPropertyStr(ctx, global_obj, "md5", JS_NewCFunction(ctx, js_md5, "md5", 1));
    JS_SetPropertyStr(ctx, global_obj, "crc32", JS_NewCFunction(ctx, js_crc32, "crc32", 1));
    JS_SetPropertyStr(ctx, global_obj, "unlink", JS_NewCFunction(ctx, js_unlink, "unlink", 1));
    JS_SetPropertyStr(ctx, global_obj, "mkdir", JS_NewCFunction(ctx, js_mkdir, "mkdir", 1));
    JS_SetPropertyStr(ctx, global_obj, "dirExists", JS_NewCFunction(ctx, js_dir_exists, "dirExists", 1));
    JS_SetPropertyStr(ctx, global_obj, "fileExists", JS_NewCFunction(ctx, js_file_exists, "fileExists", 1));
    JS_SetPropertyStr(ctx, global_obj, "epochTime", JS_NewCFunction(ctx, js_epoch_time, "epochTime", 1));
    JS_SetPropertyStr(ctx, global_obj, "microTime", JS_NewCFunction(ctx, js_micro_time, "microTime", 1));
    JS_SetPropertyStr(ctx, global_obj, "apiExecute", JS_NewCFunction(ctx, js_api_execute, "apiExecute", 2));
    JS_SetPropertyStr(ctx, global_obj, "setGlobalVariable", JS_NewCFunction(ctx, js_global_set, "setGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "getGlobalVariable", JS_NewCFunction(ctx, js_global_get, "getGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "getPath", JS_NewCFunction(ctx, js_get_path, "getPath", 1));
    JS_SetPropertyStr(ctx, global_obj, "getUUID", JS_NewCFunction(ctx, js_get_uuid, "getUUID", 1));
    JS_SetPropertyStr(ctx, global_obj, "chatSend", JS_NewCFunction(ctx, js_chat_send, "chatSend", 1));

    if(script->session) {
        script->fl_ready = true;

        session_obj = js_session_object_create(ctx, script->session);
        if(JS_IsException(session_obj)) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create session object\n");
            goto out;
        }

        JS_SetPropertyStr(ctx, global_obj, "session", session_obj);
    }

    script->fl_ready = true;
    result = JS_Eval(ctx, script->script_buf, script->script_len, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);

    if(JS_IsException(result)) {
        js_ctx_dump_error(script, ctx);
        JS_ResetUncatchableError(ctx);
    }

    JS_FreeValue(ctx, result);

out:
    JS_FreeValue(ctx, global_obj);

    script->fl_destroyed = true;
    script_wait_unlock(script);

    if(ctx) { JS_FreeContext(ctx); }
    if(rt)  { JS_FreeRuntime(rt); }

    /* ready must be changed only after rt/ctx been destroyed!  */
    /* Otherwise it corrupts js_session                         */
    script->fl_ready = false;

    switch_mutex_lock(globals.mutex_scripts_map);
    switch_core_hash_delete(globals.scripts_map, script->id);
    switch_mutex_unlock(globals.mutex_scripts_map);

    if(script->mod_hlist) {
        js_list_destroy(&script->mod_hlist);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(thread) {
        thread_finished();
    }

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
#define CMD_SYNTAX "\n" \
    "list - show running scripts\n" \
    "run-bg scriptName [args] - launch the script in backgroud\n" \
    "run    scriptName [args] - launch the script\n" \
    "int    scriptId - interrupt script\n"

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
                    stream->write_function(stream, "%s (%s) [session: %s]\n", script->id, script->path, (script->session_id ? script->session_id : "none"));
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

        if((status = script_launch(session, argv[1], script_args, NULL, false)) != SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "-ERR: %i\n", status);
        }

        goto out;
    }
    if(strcasecmp(argv[0], "run-bg") == 0) {
        char *script_args = (argc > 2 ? ((char *)cmd + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);
        char *script_id = NULL;

        new_uuid(&script_id, NULL);

        if((status = script_launch(session, argv[1], script_args, script_id, true)) == SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "+OK: %s\n", script_id);
        } else {
            stream->write_function(stream, "-ERR: %i\n", status);
        }

        switch_safe_free(script_id);
        goto out;
    }
    if(strcasecmp(argv[0], "int") == 0) {
        script_t *script = NULL;
        char *id = (argc > 1 ? argv[1] : NULL);
        int success = 0;

        script = script_lookup(id);
        if(script_sem_take(script)) {
            if(script->fl_ready && !script->fl_destroyed) {
                script->fl_interrupt = true;
                success++;
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

    if((status = script_launch(session, script_name, script_args, NULL, false)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to launch script (%s)\n", script_name);
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
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to open config (%s)\n", CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    if((xml_settings = switch_xml_child(cfg, "settings"))) {
        for(xml_param = switch_xml_child(xml_settings, "param"); xml_param; xml_param = xml_param->next) {
            char *var = (char *) switch_xml_attr_soft(xml_param, "name");
            char *val = (char *) switch_xml_attr_soft(xml_param, "value");

            if(!strcasecmp(var, "rt-stack-size-max")) {
                size_t x = atoi(val);
                if(x > 0) globals.cfg_rt_stk_size = x * 1024 * 1024;
            } else if(!strcasecmp(var, "rt-memory-limit")) {
                size_t x = atoi(val);
                if(x > 0) globals.cfg_rt_mem_limit = x * 1024 * 1024;
            }
        }
    }
    if((xml_scripts = switch_xml_child(cfg, "autoload-scripts"))) {
        for(xml_script = switch_xml_child(xml_scripts, "script"); xml_script; xml_script = xml_script->next) {
            char *path = (char *) switch_xml_attr_soft(xml_script, "path");
            char *args = (char *) switch_xml_attr_soft(xml_script, "args");

            if(!zstr(path)) {
                if(script_launch(NULL, path, args, NULL, true) != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to launch script (%s)", path);
                }
            }
        }
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(cmd_interface, "qjs", "quickjs", quickjs_cmd, CMD_SYNTAX);
    SWITCH_ADD_APP(app_interface, "qjs", "quickjs", "quickjs", quickjs_app, APP_SYNTAX, SAF_NONE);

    globals.fl_shutdown = false;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_quckjs (%s) [%s]\n", MOD_VERSION, MOD_RT_TYPE);

done:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown) {
    switch_hash_index_t *hi = NULL;
    script_t *script = NULL;
    uint8_t fl_wloop = true;
    void *hval = NULL;

    globals.fl_shutdown = true;

    switch_mutex_lock(globals.mutex_scripts_map);
    for(hi = switch_core_hash_first_iter(globals.scripts_map, hi); hi; hi = switch_core_hash_next(&hi)) {
        switch_core_hash_this(hi, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            script->fl_interrupt = true;
            script_sem_release(script);
        }
    }
    switch_safe_free(hi);
    switch_mutex_unlock(globals.mutex_scripts_map);

    switch_mutex_lock(globals.mutex);
    fl_wloop = (globals.active_threads > 0);
    switch_mutex_unlock(globals.mutex);

    if(fl_wloop) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Waiting for termination '%d' threads...\n", globals.active_threads);
        while(fl_wloop) {
            switch_mutex_lock(globals.mutex);
            fl_wloop = (globals.active_threads > 0);
            switch_mutex_unlock(globals.mutex);
            switch_yield(100000);
        }
    }

    switch_mutex_lock(globals.mutex_scripts_map);
    switch_core_hash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts_map);

    return SWITCH_STATUS_SUCCESS;
}



