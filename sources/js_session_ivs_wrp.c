/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

typedef struct {
    uint32_t                jid;
    uint8_t                 fl_delete_file;
    uint8_t                 mode; // 0 - playback, 1-say
    char                    *data;
    char                    *lang;
    js_session_t            *js_session;
    switch_memory_pool_t    *pool;
} ivs_async_playback_param_t;

static void *SWITCH_THREAD_FUNC ivs_async_playback_thread(switch_thread_t *thread, void *obj) {
    volatile ivs_async_playback_param_t *_ref = (ivs_async_playback_param_t *) obj;
    ivs_async_playback_param_t *params = (ivs_async_playback_param_t *) _ref;
    switch_memory_pool_t *pool_local = params->pool;

    if(params->mode == 1) {
        ivs_event_push(IVS_EVENTSQ(params->js_session), params->jid, IVS_EVENT_PLAYBACK_START, "SAY", 3);
        ivs_say(params->js_session, params->lang, params->data, false);
        ivs_event_push(IVS_EVENTSQ(params->js_session), params->jid, IVS_EVENT_PLAYBACK_STOP, "SAY", 3);
    } else {
        ivs_event_push(IVS_EVENTSQ(params->js_session), params->jid, IVS_EVENT_PLAYBACK_START, params->data, strlen(params->data));
        ivs_playback(params->js_session, params->data, false);
        ivs_event_push(IVS_EVENTSQ(params->js_session), params->jid, IVS_EVENT_PLAYBACK_STOP, params->data, strlen(params->data));
    }

    // relese sem
    js_session_release(params->js_session);

    if(params->fl_delete_file) {
        unlink(params->data);
    }

    if(pool_local) {
        switch_core_destroy_memory_pool(&pool_local);
    }

    thread_finished();
    return NULL;
}

uint32_t ivs_async_playback(js_session_t *js_session, const char *path, uint8_t delete_file) {
    uint32_t jid = JID_NONE;
    ivs_async_playback_param_t *params = NULL;
    switch_memory_pool_t *pool_local = NULL;

    if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        goto out;
    }
    if((params = switch_core_alloc(pool_local, sizeof(ivs_async_playback_param_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto out;
    }

    params->jid = js_session_gen_job_id(js_session);
    params->pool = pool_local;
    params->js_session = js_session;
    params->data = switch_core_strdup(pool_local, path);
    params->fl_delete_file = delete_file;
    params->mode = 0;

    if(js_session_take(params->js_session)) {
        jid = params->jid;
        launch_thread(pool_local, ivs_async_playback_thread, params);
    }
out:
    if(jid == JID_NONE) {
        if(pool_local) { switch_core_destroy_memory_pool(&pool_local); }
    }
    return jid;
}

uint32_t ivs_async_say(js_session_t *js_session, const char *lang, const char *text) {
    uint32_t jid = JID_NONE;
    ivs_async_playback_param_t *params = NULL;
    switch_memory_pool_t *pool_local = NULL;

    if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        return JID_NONE;
    }
    if((params = switch_core_alloc(pool_local, sizeof(ivs_async_playback_param_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        switch_core_destroy_memory_pool(&pool_local);
        return JID_NONE;
    }

    params->jid = js_session_gen_job_id(js_session);
    params->pool = pool_local;
    params->js_session = js_session;
    params->data = switch_core_strdup(pool_local, text);
    params->lang = safe_pool_strdup(pool_local, lang);
    params->mode = 1;

    if(js_session_take(params->js_session)) {
        jid = params->jid;
        launch_thread(pool_local, ivs_async_playback_thread, params);
    }
out:
    if(jid == JID_NONE) {
        if(pool_local) { switch_core_destroy_memory_pool(&pool_local); }
    }
    return jid;
}

