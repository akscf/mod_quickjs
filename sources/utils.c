/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include <mod_quickjs.h>

extern globals_t globals;

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data) {
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;

    switch_mutex_lock(globals.mutex);
    globals.active_threads++;
    switch_mutex_unlock(globals.mutex);

    switch_threadattr_create(&attr, pool);
    switch_threadattr_detach_set(attr, 1);
    switch_threadattr_stacksize_set(attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, attr, fun, data, pool);

    return;
}

void thread_finished() {
    switch_mutex_lock(globals.mutex);
    if(globals.active_threads > 0) { globals.active_threads--; }
    switch_mutex_unlock(globals.mutex);
}

// -----------------------------------------------------------------------------------------------------------------------------------------
switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool) {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

    memset((char *)uuid_str, '\0' , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));

    *uuid = switch_core_strdup(pool, uuid_str);

    return SWITCH_STATUS_SUCCESS;
}

script_t *script_lookup(char *id) {
    script_t *script = NULL;

    if(zstr(id)) {
        return NULL;
    }

    switch_mutex_lock(globals.mutex_scripts_map);
    script = switch_core_hash_find(globals.scripts_map, id);
    switch_mutex_unlock(globals.mutex_scripts_map);

    return script;
}

uint32_t script_sem_take(script_t *script) {
    uint32_t status = false;

    if(!script) { return false; }

    switch_mutex_lock(script->mutex);
    if(script->fl_ready) {
        status = true;
        script->sem++;
    }
    switch_mutex_unlock(script->mutex);
    return status;
}

void script_sem_release(script_t *script) {
    switch_assert(script);

    switch_mutex_lock(script->mutex);
    if(script->sem) {
        script->sem--;
    }
    switch_mutex_unlock(script->mutex);
}

void script_wait_unlock(script_t *script) {
    uint8_t fl_wloop = true;

    switch_mutex_lock(script->mutex);
    fl_wloop = (script->sem != 0);
    switch_mutex_unlock(script->mutex);

    if(fl_wloop) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for unlock (scipt-id=%s, sem=%i)\n", script->id, script->sem);
        while(fl_wloop) {
            switch_mutex_lock(script->mutex);
            fl_wloop = (script->sem != 0);
            switch_mutex_unlock(script->mutex);
            switch_yield(100000);
        }
    }
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

        switch_mutex_lock(script->classes_map_mutex);
        switch_core_hash_insert(script->classes_map, class_name, ptr);
        switch_mutex_unlock(script->classes_map_mutex);

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
        switch_mutex_lock(script->classes_map_mutex);
        ptr = switch_core_hash_find(script->classes_map, class_name);
        if(ptr) {
            id = (JSClassID) *ptr;
        }
        switch_mutex_unlock(script->classes_map_mutex);
        script_sem_release(script);
    }

    return id;
}

void js_ctx_dump_error(script_t *script, JSContext *ctx) {
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

        if(err_str) {
            JS_FreeCString(ctx, err_str);
        }
        if(stk_str) {
            JS_FreeCString(ctx, stk_str);
        }

        JS_FreeValue(ctx, stk_val);
    }

    JS_FreeValue(ctx, exception_val);
}

char *safe_pool_strdup(switch_memory_pool_t *pool, const char *str) {
    switch_assert(pool);
    if(zstr(str)) { return NULL; }
    return switch_core_strdup(pool, str);
}

uint8_t *safe_pool_bufdup(switch_memory_pool_t *pool, uint8_t *buffer, switch_size_t len) {
    uint8_t *buffer_local = NULL;
    switch_assert(pool);

    if(buffer && len > 0) {
        buffer_local = switch_core_alloc(pool, len);
        if(buffer_local == NULL) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mem fail\n");
            return NULL;
        }
        memcpy(buffer_local, buffer, len);
    }
    return buffer_local;
}

char *audio_file_write(switch_byte_t *buf, uint32_t buf_len, uint32_t samplerate, uint32_t channels, const char *file_ext) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_size_t len = (buf_len / sizeof(uint16_t));
    switch_file_handle_t fh = { 0 };
    char *file_name = NULL;
    char name_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1] = { 0 };
    int flags = (SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT);

    switch_uuid_str((char *)name_uuid, sizeof(name_uuid));
    file_name = switch_mprintf("%s%s%s.%s", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, name_uuid, (file_ext == NULL ? "wav" : file_ext) );

    if((status = switch_core_file_open(&fh, file_name, channels, samplerate, flags, NULL)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open failed (%s)\n", file_name);
        goto out;
    }

    if((status = switch_core_file_write(&fh, buf, &len)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write failed (%s)\n", file_name);
        goto out;
    }

    switch_core_file_close(&fh);
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(file_name) {
            unlink(file_name);
            switch_safe_free(file_name);
        }
        return NULL;
    }
    return file_name;
}

