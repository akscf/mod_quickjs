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
    if(globals.active_threads) globals.active_threads--;
    switch_mutex_unlock(globals.mutex);
}

switch_status_t new_uuid(char **uuid, switch_memory_pool_t *pool) {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];

    memset((char *)uuid_str, 0x0 , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));

    if(pool) {
        *uuid = switch_core_strdup(pool, uuid_str);
    } else {
        *uuid = (char *)strdup(uuid_str);
    }

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

void js_ctx_dump_error(script_t *script, JSContext *ctx) {
    if(script && script->fl_exit) {
        return;
    } else {
        JSValue exception_val = JS_GetException(ctx);
        JSValue stk_val = JS_GetPropertyStr(ctx, exception_val, "stack");
        const char *err_str = JS_ToCString(ctx, exception_val);
        const char *stk_str = (JS_IsUndefined(stk_val) ? NULL : JS_ToCString(ctx, stk_val));

        if(script) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QJS [%s/%s]: \n%s %s\n", script->name, script->id, (err_str ? err_str : "[no error message]"), stk_str);
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "QJS: %s %s\n", (err_str ? err_str : "[no error message]"), stk_str);
        }

        JS_FreeCString(ctx, err_str);
        JS_FreeCString(ctx, stk_str);
        JS_FreeValue(ctx, stk_val);
        JS_FreeValue(ctx, exception_val);
    }
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
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_alloc()\n");
            return NULL;
        }
        memcpy(buffer_local, buffer, len);
    }
    return buffer_local;
}
