/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

extern globals_t globals;

typedef struct {
    switch_memory_pool_t    *pool;
    js_session_t            *js_session;
    char                    *data;
    char                    *lang;
    uint8_t                  mode; // 0-playback, 1=say
} playback_thread_params_t;

static void *SWITCH_THREAD_FUNC playback_async_thread(switch_thread_t *thread, void *obj) {
    volatile playback_thread_params_t *_ref = (playback_thread_params_t *) obj;
    playback_thread_params_t *params = (playback_thread_params_t *) _ref;
    switch_memory_pool_t *pool_local = params->pool;

    if(params->mode == 1) {
        ivs_say(params->js_session, params->lang, params->data, false);
    } else {
        ivs_playback(params->js_session, params->data, false);
    }

    if(pool_local) {
        switch_core_destroy_memory_pool(&pool_local);
    }

    thread_finished();
    return NULL;
}

static switch_status_t read_frame_callback(switch_core_session_t *session, switch_frame_t *frame, void *user_data) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    js_session_t *js_session = (js_session_t *) user_data;

    if(js_session->fl_ready && js_session->fl_audio_cap_cative && frame && frame->datalen > 0) {
        ivs_audio_frame_push(IVS_AUDIOQ(js_session), frame->data, frame->datalen, js_session->samplerate, js_session->channels);
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t on_dtmf_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
    js_session_t *js_session = (js_session_t *) buf;

    switch(itype) {
        case SWITCH_INPUT_TYPE_DTMF: {
                switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
                ivs_audio_frame_push(IVS_DTMFQ(js_session), (uint8_t *) &dtmf->digit, 1, 0, 0);
            }
            break;
        default:
            break;
    }

    return SWITCH_STATUS_SUCCESS;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t ivs_playback_stop(js_session_t *js_session) {
    switch_status_t  status = SWITCH_STATUS_SUCCESS;
    int x = 0;

    switch_assert(js_session);

    if(js_session_take(js_session)) {

        if(js_session_xflags_test(js_session, IVS_XFLAG_PLAYBACK)) {
            switch_channel_set_flag(switch_core_session_get_channel(js_session->session), CF_BREAK);

            while(js_session_xflags_test(js_session, IVS_XFLAG_PLAYBACK)) {
                if(globals.fl_shutdown || !js_session->fl_ready) { break; }
                if(x > 500) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't stop playback (session: %s)\n", js_session->session_id);
                    status = SWITCH_STATUS_FALSE;
                    break;
                }
                x++;
                switch_yield(10000);
            }
        }
        done:
        js_session_release(js_session);
    }

    return status;
}

switch_status_t ivs_say(js_session_t *js_session, char *language, char *text, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_input_args_t *ap = NULL;
    switch_input_args_t args = { 0 };
    switch_channel_t *channel = NULL;
    const char *engine_local = NULL;
    const char *language_local = language;
    char *expanded = NULL;

    switch_assert(js_session);

    if(zstr(text)) { return SWITCH_STATUS_FALSE; }

    if(async) {
        switch_memory_pool_t *pool_local = NULL;
        playback_thread_params_t *params = NULL;

        if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
            return SWITCH_STATUS_FALSE;
        }
        if((params = switch_core_alloc(pool_local, sizeof(playback_thread_params_t))) == NULL) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            switch_core_destroy_memory_pool(&pool_local);
            return SWITCH_STATUS_FALSE;
        }

        params->pool = pool_local;
        params->js_session = js_session;
        params->data = switch_core_strdup(pool_local, text);
        params->lang = (language == NULL ? NULL : switch_core_strdup(pool_local, language));
        params->mode = 1;

        launch_thread(pool_local, playback_async_thread, params);
        return SWITCH_STATUS_SUCCESS;
    }

    args.read_frame_callback = read_frame_callback;
    args.user_data = js_session;
    args.input_callback = on_dtmf_callback;
    args.buf = js_session;
    args.buflen = 1;
    ap = &args;

    if(js_session_take(js_session)) {
        channel = switch_core_session_get_channel(js_session->session);

        if((expanded = switch_channel_expand_variables(channel, text)) != text) {
            text = expanded;
        } else {
            expanded = NULL;
        }

        if(js_session_xflags_test(js_session, IVS_XFLAG_PLAYBACK)) {
            if((status = ivs_playback_stop(js_session)) != SWITCH_STATUS_SUCCESS) {
                js_session_release(js_session);
                goto out;
            }
        }

        js_session_xflags_set(js_session, IVS_XFLAG_PLAYBACK, true);

        if(switch_channel_test_flag(channel, CF_BREAK)) {
            switch_channel_clear_flag(channel, CF_BREAK);
        }

        for(int x = 0; x < 10; x++) {
            switch_frame_t *read_frame;
            status = switch_core_session_read_frame(js_session->session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
            if(!SWITCH_READ_ACCEPTABLE(status)) { break; }
        }

        if(!engine_local) {
            engine_local = switch_channel_get_variable(channel, "tts_engine");
        }
        if(!language_local) {
            language_local = switch_channel_get_variable(channel, "tts_language");
        }

        if(engine_local && language_local) {
            status = switch_ivr_speak_text(js_session->session, engine_local, language_local, text, &args);
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts engine or language not defined\n");
            status = SWITCH_STATUS_FALSE;
        }

        js_session_xflags_set(js_session, IVS_XFLAG_PLAYBACK, false);
        js_session_release(js_session);
    }

out:
    switch_safe_free(expanded);
    return status;
}

switch_status_t ivs_playback(js_session_t *js_session, char *path, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_input_args_t *ap = NULL;
    switch_input_args_t args = { 0 };
    switch_channel_t *channel = NULL;
    const char *engine_local = NULL;
    const char *language_local = NULL;
    char *expanded = NULL;

    switch_assert(js_session);

    if(zstr(path)) { return SWITCH_STATUS_NOTFOUND; }

    if(async) {
        switch_memory_pool_t *pool_local = NULL;
        playback_thread_params_t *params = NULL;

        if(switch_core_new_memory_pool(&pool_local) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
            return SWITCH_STATUS_FALSE;
        }
        if((params = switch_core_alloc(pool_local, sizeof(playback_thread_params_t))) == NULL) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            switch_core_destroy_memory_pool(&pool_local);
            return SWITCH_STATUS_FALSE;
        }

        params->pool = pool_local;
        params->js_session = js_session;
        params->data = switch_core_strdup(pool_local, path);
        params->mode = 0;

        launch_thread(pool_local, playback_async_thread, params);
        return SWITCH_STATUS_SUCCESS;
    }

    args.read_frame_callback = read_frame_callback;
    args.user_data = js_session;
    args.input_callback = on_dtmf_callback;
    args.buf = js_session;
    args.buflen = 1;
    ap = &args;

    if(js_session_take(js_session)) {
        channel = switch_core_session_get_channel(js_session->session);

        if((expanded = switch_channel_expand_variables(channel, path)) != path) {
            path = expanded;
        } else {
            expanded = NULL;
        }

        if(js_session_xflags_test(js_session, IVS_XFLAG_PLAYBACK)) {
            if((status = ivs_playback_stop(js_session)) != SWITCH_STATUS_SUCCESS) {
                js_session_release(js_session);
                goto out;
            }
        }

        js_session_xflags_set(js_session, IVS_XFLAG_PLAYBACK, true);

        if(switch_channel_test_flag(channel, CF_BREAK)) {
            switch_channel_clear_flag(channel, CF_BREAK);
        }

        for(int x = 0; x < 10; x++) {
            switch_frame_t *read_frame;
            status = switch_core_session_read_frame(js_session->session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
            if(!SWITCH_READ_ACCEPTABLE(status)) { break; }
        }

        if(strncasecmp(path, "say://", 4) == 0) {
            if(!engine_local) {
                engine_local = switch_channel_get_variable(channel, "tts_engine");
            }
            if(!language_local) {
                language_local = switch_channel_get_variable(channel, "tts_language");
            }

            if(engine_local && language_local) {
                status = switch_ivr_speak_text(js_session->session, engine_local, language_local, path + 6, &args);
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "tts engine or language not defined\n");
                status = SWITCH_STATUS_FALSE;
            }
        } else if(strstr(path, "://") != NULL) {
            status = switch_ivr_play_file(js_session->session, NULL, path, ap);
        } else {
            if(switch_file_exists(path, NULL) == SWITCH_STATUS_SUCCESS) {
                status = switch_ivr_play_file(js_session->session, NULL, path, ap);
            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "file not found: %s\n", path);
                status = SWITCH_STATUS_NOTFOUND;
            }
        }

        js_session_xflags_set(js_session, IVS_XFLAG_PLAYBACK, false);
        js_session_release(js_session);
    }

out:
    switch_safe_free(expanded);
    return status;
}
