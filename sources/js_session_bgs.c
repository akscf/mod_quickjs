/**
 * (C)2025 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

extern globals_t globals;

typedef struct {
    char                    *data;
    switch_memory_pool_t    *pool;
    js_session_t            *js_session;
} bg_playback_param_t;

static switch_status_t session_playback_perform(js_session_t *jss, switch_memory_pool_t *pool, char *path);

static void *SWITCH_THREAD_FUNC bg_playback_thread(switch_thread_t *thread, void *obj) {
    volatile bg_playback_param_t *_ref = (bg_playback_param_t *) obj;
    bg_playback_param_t *params = (bg_playback_param_t *) _ref;
    switch_memory_pool_t *pool_local = params->pool;
    js_session_t *jss = params->js_session;

    session_playback_perform(jss, pool_local, params->data);

    if(pool_local) {
        switch_core_destroy_memory_pool(&pool_local);
    }

    js_session_release(jss);
    thread_finished();
    return NULL;
}

static switch_status_t session_playback_perform(js_session_t *jss, switch_memory_pool_t *pool, char *path) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_channel_t *channel = NULL;
    const char *engine_local = NULL;
    const char *language_local = NULL;
    switch_file_handle_t fh = {0};
    char *expanded = NULL;

    switch_assert(jss);

    if(zstr(path)) {
        return SWITCH_STATUS_NOTFOUND;
    }

    if(js_session_take(jss)) {
        channel = switch_core_session_get_channel(jss->session);

        if((expanded = switch_channel_expand_variables(channel, path)) != path) {
            path = expanded;
        } else {
            expanded = NULL;
        }

        if(jss->bg_streams) {
            if((status = js_session_bgs_stream_stop(jss)) != SWITCH_STATUS_SUCCESS) {
                js_session_release(jss);
                goto out;
            }
        }

        switch_mutex_lock(jss->mutex);
        jss->bg_streams++;
        jss->bg_stream_fh = &fh;
        switch_mutex_unlock(jss->mutex);

        if(switch_channel_test_flag(channel, CF_BREAK)) {
            switch_channel_clear_flag(channel, CF_BREAK);
        }

        for(int x = 0; x < 5; x++) {
            switch_frame_t *read_frame = NULL;
            status = switch_core_session_read_frame(jss->session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
            if(!SWITCH_READ_ACCEPTABLE(status)) { break; }
        }

        if(strncasecmp(path, "say://", 4) == 0) {
            if(!engine_local) {
                engine_local = switch_channel_get_variable(channel, "tts_engine");
            }
            if(!language_local) {
                language_local = switch_channel_get_variable(channel, "language");
            }

            if(engine_local && language_local) {
                status = switch_ivr_speak_text(jss->session, engine_local, language_local, path + 6, NULL);
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing channel variable: tts_engine or language\n");
                status = SWITCH_STATUS_FALSE;
            }
        } else if(strstr(path, "://") != NULL) {
            status = switch_ivr_play_file(jss->session, &fh, path, NULL);
        } else {
            if(switch_file_exists(path, NULL) == SWITCH_STATUS_SUCCESS) {
                status = switch_ivr_play_file(jss->session, &fh, path, NULL);
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found (%s)\n", path);
                status = SWITCH_STATUS_NOTFOUND;
            }
        }


        switch_mutex_lock(jss->mutex);
        if(jss->bg_streams) jss->bg_streams--;
        jss->bg_stream_fh = NULL;
        switch_mutex_unlock(jss->mutex);

        js_session_release(jss);
    }

out:
    switch_safe_free(expanded);
    return status;
}




// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// public
// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t js_session_bgs_stream_start(js_session_t *jss, const char *path) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    bg_playback_param_t *params = NULL;
    switch_memory_pool_t *pool_local = NULL;

    if((status = switch_core_new_memory_pool(&pool_local)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto out;
    }

    if(!(params = switch_core_alloc(pool_local, sizeof(bg_playback_param_t)))) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_alloc()\n");
        goto out;
    }

    params->pool = pool_local;
    params->js_session = jss;
    params->data = switch_core_strdup(pool_local, path);

    if(js_session_take(params->js_session)) {
        launch_thread(pool_local, bg_playback_thread, params);
    }

out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(pool_local) {
            switch_core_destroy_memory_pool(&pool_local);
        }
    }

    return status;
}

switch_status_t js_session_bgs_stream_stop(js_session_t *jss) {
    switch_status_t  status = SWITCH_STATUS_SUCCESS;
    uint32_t x = 0;

    if(jss->bg_streams) {
        switch_channel_set_flag(switch_core_session_get_channel(jss->session), CF_BREAK);

        while(jss->bg_streams) {
            if(globals.fl_shutdown || !jss->fl_ready) {
                break;
            }
            if(++x > 500) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to stop background stream (session: %s)\n", jss->session_id);
                status = SWITCH_STATUS_FALSE;
                break;
            }
            switch_yield(10000);
        }
    }

    return status;
}
