/**
 * (C)2024 aks
 * https://github.com/akscf/
 **/
#include "js_ivs.h"

typedef struct {
    uint32_t                jid;
    uint8_t                 fl_delete_file;
    uint8_t                 mode;           // 0 - playback, 1-say
    char                    *data;
    char                    *lang;
    js_ivs_t                *js_ivs;
    switch_memory_pool_t    *pool;
} js_ivs_async_playback_param_t;

static void *SWITCH_THREAD_FUNC js_ivs_async_playback_thread(switch_thread_t *thread, void *obj) {
    volatile js_ivs_async_playback_param_t *_ref = (js_ivs_async_playback_param_t *) obj;
    js_ivs_async_playback_param_t *params = (js_ivs_async_playback_param_t *) _ref;
    switch_memory_pool_t *pool_local = params->pool;
    js_ivs_t *js_ivs = params->js_ivs;

    if(params->mode == 1) {
        js_ivs_event_push(IVS_EVENTSQ(js_ivs), params->jid, IVS_EVENT_PLAYBACK_START, "SAY", 3);
        js_ivs_say(js_ivs, params->lang, params->data, false);
        js_ivs_event_push(IVS_EVENTSQ(js_ivs), params->jid, IVS_EVENT_PLAYBACK_STOP, "SAY", 3);
    } else {
        js_ivs_event_push(IVS_EVENTSQ(js_ivs), params->jid, IVS_EVENT_PLAYBACK_START, params->data, strlen(params->data));
        js_ivs_playback(js_ivs, params->data, false);
        js_ivs_event_push(IVS_EVENTSQ(js_ivs), params->jid, IVS_EVENT_PLAYBACK_STOP, params->data, strlen(params->data));
    }

    if(params->fl_delete_file) {
        unlink(params->data);
    }

    if(pool_local) {
        switch_core_destroy_memory_pool(&pool_local);
    }

    js_ivs_release(js_ivs);
    thread_finished();
    return NULL;
}

uint32_t js_ivs_async_playback(js_ivs_t *js_ivs, const char *path, uint8_t delete_file) {
    uint32_t jid = JID_NONE;
    js_ivs_async_playback_param_t *params = NULL;
    switch_memory_pool_t *pool_local = NULL;

    if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        goto out;
    }
    if((params = switch_core_alloc(pool_local, sizeof(js_ivs_async_playback_param_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto out;
    }

    params->jid = js_ivs_gen_job_id(js_ivs);
    params->pool = pool_local;
    params->js_ivs = js_ivs;
    params->data = switch_core_strdup(pool_local, path);
    params->fl_delete_file = delete_file;
    params->mode = 0;

    if(js_ivs_take(params->js_ivs)) {
        jid = params->jid;
        launch_thread(pool_local, js_ivs_async_playback_thread, params);
    }
out:
    if(jid == JID_NONE) {
        if(pool_local) {
            switch_core_destroy_memory_pool(&pool_local);
        }
    }
    return jid;
}

uint32_t js_ivs_async_say(js_ivs_t *js_ivs, const char *lang, const char *text) {
    uint32_t jid = JID_NONE;
    js_ivs_async_playback_param_t *params = NULL;
    switch_memory_pool_t *pool_local = NULL;

    if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        goto out;
    }
    if((params = switch_core_alloc(pool_local, sizeof(js_ivs_async_playback_param_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto out;
    }

    params->jid = js_ivs_gen_job_id(js_ivs);
    params->pool = pool_local;
    params->js_ivs = js_ivs;
    params->data = switch_core_strdup(pool_local, text);
    params->lang = safe_pool_strdup(pool_local, lang);
    params->mode = 1;

    if(js_ivs_take(params->js_ivs)) {
        jid = params->jid;
        launch_thread(pool_local, js_ivs_async_playback_thread, params);
    }
out:
    if(jid == JID_NONE) {
        if(pool_local) {
            switch_core_destroy_memory_pool(&pool_local);
        }
    }
    return jid;
}

