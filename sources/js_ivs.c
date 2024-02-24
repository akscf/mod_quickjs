/**
 * (C)2024 aks
 * https://github.com/akscf/
 **/
#include "js_ivs.h"

#define CLASS_NAME                          "IVS"
#define PROP_SESSION_ID                     1
#define PROP_CHUNK_SEC                      2
#define PROP_CHUNK_TYPE                     3
#define PROP_CHUNK_ENCODING                 4
#define PROP_VAD_VOICE_MS                   5
#define PROP_VAD_SILENCE_MS                 6
#define PROP_VAD_THRESHOLD                  7
#define PROP_VAD_DEBUG                      8
#define PROP_VAD_STATE                      9
#define PROP_CNG_ENABLED                    10
#define PROP_CNG_LEVEL                      11
#define PROP_DTMF_MAX_DIGITS                12
#define PROP_DTMF_MAX_IDLE                  13
#define PROP_SILENCE_TIMOUT                 14
#define PROP_SESSION_TIMOUT                 15
#define PROP_TTS_ENGINE                     16
#define PROP_ASR_ENGINE                     17
#define PROP_LANGUAGE                       18
#define PROP_IS_AUDIO_CAPTURE_ACTIVE        30
#define PROP_IS_VIDEO_CAPTURE_ACTIVE        31
#define PROP_IS_AUDIO_CAPTURE_ON_PAUSE      32
#define PROP_IS_VIDEO_CAPTURE_ON_PAUSE      33

static void js_ivs_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_ivs_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));

    if(!js_ivs) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_SESSION_ID: {
            return JS_NewString(ctx, js_ivs->js_session->session_id);
        }
        case PROP_CHUNK_SEC: {
            return JS_NewInt32(ctx, js_ivs->audio_chunk_sec);
        }
        case PROP_CHUNK_TYPE: {
            return JS_NewString(ctx, js_ivs_chunkType2name(js_ivs->audio_chunk_type));
        }
        case PROP_CHUNK_ENCODING: {
            return JS_NewString(ctx, js_ivs_chunkEncoding2name(js_ivs->audio_chunk_encoding));
        }
        case PROP_VAD_VOICE_MS: {
            return JS_NewInt32(ctx, js_ivs->vad_voice_ms);
        }
        case PROP_VAD_SILENCE_MS: {
            return JS_NewInt32(ctx, js_ivs->vad_silence_ms);
        }
        case PROP_VAD_THRESHOLD: {
            return JS_NewInt32(ctx, js_ivs->vad_threshold);
        }
        case PROP_VAD_DEBUG: {
            return (js_ivs->vad_debug ? JS_TRUE : JS_FALSE);
        }
        case PROP_VAD_STATE: {
            return JS_NewString(ctx, js_ivs_vadState2name(js_ivs->vad_state));
        }
        case PROP_CNG_ENABLED: {
            return (js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_AUDIO_CNG_ENABLED) ? JS_TRUE : JS_FALSE);
        }
        case PROP_CNG_LEVEL: {
            return JS_NewInt32(ctx, js_ivs->cng_lvl);
        }
        case PROP_DTMF_MAX_DIGITS: {
            return JS_NewInt32(ctx, js_ivs->dtmf_max_digits);
        }
        case PROP_DTMF_MAX_IDLE: {
            return JS_NewInt32(ctx, js_ivs->dtmf_idle_sec);
        }
        case PROP_SILENCE_TIMOUT: {
            return JS_NewInt32(ctx, js_ivs->silence_timeout_sec);
        }
        case PROP_SESSION_TIMOUT: {
            return JS_NewInt32(ctx, js_ivs->session_timeout_sec);
        }
        case PROP_TTS_ENGINE: {
            return JS_NewString(ctx, js_ivs->tts_engine);
        }
        case PROP_ASR_ENGINE: {
            return JS_NewString(ctx, js_ivs->asr_engine);
        }
        case PROP_LANGUAGE: {
            return JS_NewString(ctx, js_ivs->language);
        }
        case PROP_IS_AUDIO_CAPTURE_ACTIVE: {
            return (js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_AUDIO_CAP_ACTIVE) ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_VIDEO_CAPTURE_ACTIVE: {
            return (js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_VIDEO_CAP_ACTIVE) ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_AUDIO_CAPTURE_ON_PAUSE: {
            return (js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_AUDIO_CAP_PAUSE) ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_VIDEO_CAPTURE_ON_PAUSE: {
            return (js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_VIDEO_CAP_PAUSE) ? JS_TRUE : JS_FALSE);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_ivs_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    uint32_t ival = 0;
    const char *str = NULL;
    int copy = 1, success = 1;

    if(!js_ivs) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_CHUNK_SEC: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &ival, val);
            if(ival >= 1) {
                js_ivs->audio_chunk_sec = ival;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_CHUNK_TYPE: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            str = JS_ToCString(ctx, val);
            js_ivs->audio_chunk_encoding = js_ivs_chunkType2id(str);
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_CHUNK_ENCODING: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            str = JS_ToCString(ctx, val);
            js_ivs->audio_chunk_type = js_ivs_chunkEncoding2id(str);
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_VAD_VOICE_MS: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &js_ivs->vad_voice_ms, val);
            return JS_TRUE;
        }
        case PROP_VAD_SILENCE_MS: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &js_ivs->vad_silence_ms, val);
            return JS_TRUE;
        }
        case PROP_VAD_THRESHOLD: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &js_ivs->vad_threshold, val);
            return JS_TRUE;
        }
        case PROP_VAD_DEBUG: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            js_ivs->vad_debug = JS_ToBool(ctx, val);
            return JS_TRUE;
        }
        case PROP_CNG_ENABLED: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            js_ivs_xflags_set(js_ivs, IVS_XFLAG_AUDIO_CNG_ENABLED, JS_ToBool(ctx, val));
            return JS_TRUE;
        }
        case PROP_CNG_LEVEL: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &js_ivs->cng_lvl, val);
            return JS_TRUE;
        }
        case PROP_DTMF_MAX_DIGITS: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &ival, val);
            if(ival >= 1) {
                js_ivs->dtmf_max_digits = ival;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_DTMF_MAX_IDLE: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &ival, val);
            if(ival >= 1) {
                js_ivs->dtmf_idle_sec = ival;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_SILENCE_TIMOUT: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            JS_ToUint32(ctx, &js_ivs->silence_timeout_sec, val);
            return JS_TRUE;
        }
        case PROP_SESSION_TIMOUT: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            switch_mutex_lock(js_ivs->mutex_timers);
            JS_ToUint32(ctx, &js_ivs->session_timeout_sec, val);
            switch_mutex_unlock(js_ivs->mutex_timers);
            return JS_TRUE;
        }
        case PROP_TTS_ENGINE: {
            switch_mutex_lock(js_ivs->mutex);
            if(QJS_IS_NULL(val)) { js_ivs->tts_engine = NULL; }
            else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_ivs->tts_engine)) { copy = strcmp(js_ivs->tts_engine, str); }
                if(copy) { js_ivs->tts_engine = switch_core_strdup(js_ivs->pool, str); }
                JS_FreeCString(ctx, str);
            }
            switch_mutex_unlock(js_ivs->mutex);
            return JS_TRUE;
        }
        case PROP_ASR_ENGINE: {
            switch_mutex_lock(js_ivs->mutex);
            if(QJS_IS_NULL(val)) {
                js_ivs->asr_engine = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_ivs->asr_engine)) { copy = strcmp(js_ivs->asr_engine, str); }
                if(copy) { js_ivs->asr_engine = switch_core_strdup(js_ivs->pool, str); }
                JS_FreeCString(ctx, str);
            }
            switch_mutex_unlock(js_ivs->mutex);
            return JS_TRUE;
        }
        case PROP_LANGUAGE: {
            switch_mutex_lock(js_ivs->mutex);
            if(QJS_IS_NULL(val)) {
                js_ivs->language = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_ivs->language)) { copy = strcmp(js_ivs->language, str); }
                if(copy) { js_ivs->language = switch_core_strdup(js_ivs->pool, str); }
                JS_FreeCString(ctx, str);
            }
            switch_mutex_unlock(js_ivs->mutex);
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

/* capture([video|audio], [[file|buffer], [wav|mp3|...])] */
static JSValue js_start_capture(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    JSValue ret_val = JS_FALSE;
    const char *what = NULL;
    const char *chunk_type = NULL;
    const char *chunk_enc = NULL;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: what");
    }

    what = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        if(!QJS_IS_NULL(argv[1])) { chunk_type = JS_ToCString(ctx, argv[1]); }
    }
    if(argc > 2) {
        if(!QJS_IS_NULL(argv[2])) { chunk_enc = JS_ToCString(ctx, argv[2]); }
    }

    if(strcasecmp(what, "audio") == 0) {
        if(chunk_type) {
            js_ivs->audio_chunk_type = js_ivs_chunkType2id(chunk_type);
        }
        if(chunk_enc) {
            js_ivs->audio_chunk_encoding = js_ivs_chunkEncoding2id(chunk_enc);
        }
        if(js_ivs_audio_start_capture(js_ivs) == SWITCH_STATUS_SUCCESS) {
            ret_val = JS_TRUE;
        }
        goto out;
    }

    if(strcasecmp(what, "video") == 0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "not yet implemented (%s)\n", what);
    }
out:
    JS_FreeCString(ctx, what);
    JS_FreeCString(ctx, chunk_type);
    JS_FreeCString(ctx, chunk_enc);
    return ret_val;
}

/* pause(audio|video) */
static JSValue js_pause_capturing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    const char *what = NULL;
    JSValue ret_val = JS_FALSE;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc > 0 && !QJS_IS_NULL(argv[0])) {
        what = JS_ToCString(ctx, argv[0]);
    }

    if(what == NULL || strcasecmp(what, "*") == 0 || strcasecmp(what, "audio") == 0) {
        js_ivs_xflags_set(js_ivs, IVS_XFLAG_AUDIO_CAP_PAUSE, true);
    }

    if(what == NULL || strcasecmp(what, "*") == 0 || strcasecmp(what, "video") == 0) {
        js_ivs_xflags_set(js_ivs, IVS_XFLAG_VIDEO_CAP_PAUSE, true);
    }

    JS_FreeCString(ctx, what);
    return ret_val;
}

/* pause(audio|video) */
static JSValue js_stop_capturing(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    const char *what = NULL;
    JSValue ret_val = JS_FALSE;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc > 0 && !QJS_IS_NULL(argv[0])) {
        what = JS_ToCString(ctx, argv[0]);
    }

    if(what == NULL || strcasecmp(what, "*") == 0 || strcasecmp(what, "audio") == 0) {
        js_ivs_xflags_set(js_ivs, IVS_XFLAG_AUDIO_CAP_DO_STOP, true);
    }

    if(what == NULL || strcasecmp(what, "*") == 0 || strcasecmp(what, "video") == 0) {
        js_ivs_xflags_set(js_ivs, IVS_XFLAG_VIDEO_CAP_DO_STOP, true);
    }

    JS_FreeCString(ctx, what);
    return ret_val;
}

static JSValue js_playback_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    js_ivs_playback_stop(js_ivs);

    return JS_TRUE;
}

// playback("fileToPlay", [delete after paly: true/false], [async: true/false])
static JSValue js_do_playback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    const char *path = NULL;
    int fl_async = false, fl_delete = false;
    JSValue ret_val = JS_UNDEFINED;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: filename");
    }
    path = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        fl_delete = JS_ToBool(ctx, argv[1]);
    }
    if(argc > 2) {
        fl_async = JS_ToBool(ctx, argv[2]);
    }

    if(fl_async) {
        uint32_t jid = js_ivs_async_playback(js_ivs, path, fl_delete);
        ret_val = (jid > 0 ? JS_NewInt32(ctx, jid) : JS_FALSE);
    } else {
        switch_status_t st = js_ivs_playback(js_ivs, (char *)path, false);
        if(fl_delete) { unlink(path); }
        ret_val = (st == SWITCH_STATUS_SUCCESS ? JS_TRUE : JS_FALSE);
    }

    JS_FreeCString(ctx, path);
    return ret_val;
}

// say("textToSpeech", [async: true/false])
static JSValue js_do_say(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    const char *text = NULL;
    int fl_async = false;
    JSValue ret_val = JS_UNDEFINED;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: text");
    }
    text = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        fl_async = JS_ToBool(ctx, argv[1]);
    }

    if(fl_async) {
        uint32_t jid = js_ivs_async_say(js_ivs, js_ivs->language, text);
        ret_val = (jid > 0 ? JS_NewInt32(ctx, jid) : JS_FALSE);
    } else {
        switch_status_t st = js_ivs_say(js_ivs, js_ivs->language, (char *)text, false);
        ret_val = (st == SWITCH_STATUS_SUCCESS ? JS_TRUE : JS_FALSE);
    }

    JS_FreeCString(ctx, text);
    return ret_val;
}

// say("textToSpeech", language, [async: true/false])
static JSValue js_do_say_with_lang(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    const char *text = NULL;
    const char *lang = NULL;
    int fl_async = false;
    JSValue ret_val = JS_UNDEFINED;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: text");
    }
    text = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        if(QJS_IS_NULL(argv[1])) {
            return JS_ThrowTypeError(ctx, "Invalid argument: language");
        }
        lang = JS_ToCString(ctx, argv[1]);
    }

    if(argc > 2) {
        fl_async = JS_ToBool(ctx, argv[2]);
    }

    if(fl_async) {
        uint32_t jid = js_ivs_async_say(js_ivs, (char *)lang, text);
        ret_val = (jid > 0 ? JS_NewInt32(ctx, jid) : JS_FALSE);
    } else {
        switch_status_t st = js_ivs_say(js_ivs, (char *)lang, (char *)text, false);
        ret_val = (st == SWITCH_STATUS_SUCCESS ? JS_TRUE : JS_FALSE);
    }

    JS_FreeCString(ctx, text);
    JS_FreeCString(ctx, lang);
    return ret_val;
}

static JSValue js_timers_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    JSValue ret_val = JS_TRUE;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(!js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_SRVC_THR_ACTIVE)) {
        if(js_ivs_service_thread_start(js_ivs) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't start service thread\n");
            ret_val = JS_FALSE;
        }
    }

    return ret_val;
}

static JSValue js_timers_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_SRVC_THR_ACTIVE)) {
        js_ivs_xflags_set(js_ivs, IVS_XFLAG_SRVC_THR_DO_STOP, true);
    }

    return JS_TRUE;
}

// timerSetup(id, intervalSec, ['once']);
static JSValue js_timer_setup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    JSValue ret_val = JS_FALSE;
    const char *mode = NULL;
    uint32_t timer_id = 0, timer_val = 0, flags = 0x0;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: timerId");
    }
    if(QJS_IS_NULL(argv[1])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: interval");
    }
    if(argc > 2 && !QJS_IS_NULL(argv[2])) {
        mode = JS_ToCString(ctx, argv[2]);
        if(mode) {
            if(strcasecmp(mode, "once") == 0) {flags = IVS_TIMER_ONCE;}
        }
    }

    JS_ToUint32(ctx, &timer_id, argv[0]);
    JS_ToUint32(ctx, &timer_val, argv[1]);

    if(timer_id > IVS_TIMERS_MAX) {
        timer_id = IVS_TIMERS_MAX;
    }

    switch_mutex_lock(js_ivs->mutex_timers);
    js_ivs->timers[timer_id].mode = flags;
    js_ivs->timers[timer_id].interval = timer_val;
    js_ivs->timers[timer_id].timer = (timer_val > 0 ? (switch_epoch_time_now(NULL) + timer_val) : 0);
    switch_mutex_unlock(js_ivs->mutex_timers);

out:
    JS_FreeCString(ctx, mode);
    return ret_val;
}

// timerCancel(id)
static JSValue js_timer_cancel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    uint32_t timer_id = 0;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: timerId");
    }
    JS_ToUint32(ctx, &timer_id, argv[0]);

    if(timer_id > IVS_TIMERS_MAX) {
        timer_id = IVS_TIMERS_MAX;
    }

    switch_mutex_lock(js_ivs->mutex_timers);
    js_ivs->timers[timer_id].interval = 0x0;
    js_ivs->timers[timer_id].timer = 0x0;
    switch_mutex_unlock(js_ivs->mutex_timers);

out:
    return JS_TRUE;
}


/* transcribe(chunkType, chunkData, samplerate, channels, [apiKey]) */
static JSValue js_do_transcribe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    JSValue ret_val = JS_FALSE;
    switch_event_t *event = NULL;
    const char *chunkType = NULL;
    const char *chunkData = NULL;
    const char *apiKey = NULL;
    switch_size_t chunk_data_size = 0;
    uint32_t samplerate=0, channels=0, rqid = 0;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }
    if(!js_ivs->asr_engine) {
        return JS_ThrowTypeError(ctx, "ASR engine not defined");
    }
    if(argc < 4) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }
    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: chunkType");
    }
    if(QJS_IS_NULL(argv[1])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: chunkData");
    }
    if(QJS_IS_NULL(argv[2])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: samperate");
    }
    if(QJS_IS_NULL(argv[3])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: channels");
    }

    return JS_ThrowTypeError(ctx, "Not yet implemented");

out:
    JS_FreeCString(ctx, apiKey);
    JS_FreeCString(ctx, chunkType);
    JS_FreeCString(ctx, chunkData);
    return ret_val;
}

static JSValue js_ivs_get_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_ivs_t *js_ivs = JS_GetOpaque2(ctx, this_val, js_ivs_get_classid(ctx));
    JSValue ret_val = JS_FALSE;
    JSValue edata_obj = JS_FALSE;
    uint8_t fl_found = false;
    void *pop = NULL;

    if(!js_ivs) {
        return JS_ThrowTypeError(ctx, "Malformed reference (js_ivs == NULL)");
    }

    if(switch_queue_trypop(IVS_EVENTSQ(js_ivs), &pop) == SWITCH_STATUS_SUCCESS) {
        js_ivs_event_t *event = (js_ivs_event_t *)pop;
        if(event) {
            fl_found = true;
            ret_val = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, ret_val, "jid",     JS_NewInt32(ctx, event->jid));
            switch(event->type) {
                case IVS_EVENT_SPEAKING_START: {
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "speaking-start"));
                    break;
                }
                case IVS_EVENT_SPEAKING_STOP: {
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "speaking-stop"));
                    break;
                }
                case IVS_EVENT_SILENCE_TIMEOUT: {
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "silence-timeout"));
                    break;
                }
                case IVS_EVENT_SESSION_TIMEOUT: {
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "session-timeout"));
                    break;
                }
                case IVS_EVENT_PLAYBACK_START: {
                    edata_obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "playback-start"));
                    JS_SetPropertyStr(ctx, ret_val, "data", edata_obj);
                    JS_SetPropertyStr(ctx, edata_obj, "file", JS_NewStringLen(ctx, event->payload, event->payload_len));
                    break;
                }
                case IVS_EVENT_PLAYBACK_STOP: {
                    edata_obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "playback-stop"));
                    JS_SetPropertyStr(ctx, ret_val, "data", edata_obj);
                    JS_SetPropertyStr(ctx, edata_obj, "file", JS_NewStringLen(ctx, event->payload, event->payload_len));
                    break;
                }
                case IVS_EVENT_DTMF_BUFFER_READY: {
                    edata_obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "dtmf-buffer-ready"));
                    JS_SetPropertyStr(ctx, ret_val, "data", edata_obj);
                    JS_SetPropertyStr(ctx, edata_obj, "digits", JS_NewStringLen(ctx, event->payload, event->payload_len));
                    break;
                }
                case IVS_EVENT_TIMER_TIMEOUT: {
                    edata_obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "timer-timeout"));
                    JS_SetPropertyStr(ctx, ret_val, "data", edata_obj);
                    JS_SetPropertyStr(ctx, edata_obj, "timer", JS_NewInt32(ctx, event->payload_len)); // it's ok, value keept in len
                    break;
                }
                case IVS_EVENT_AUDIO_CHUNK_READY: {
                    js_ivs_event_payload_audio_chunk_t *payload = (js_ivs_event_payload_audio_chunk_t *)event->payload;
                    edata_obj = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, ret_val, "type", JS_NewString(ctx, "audio-chunk-ready"));
                    JS_SetPropertyStr(ctx, ret_val, "data", edata_obj);

                    if(payload) {
                        JS_SetPropertyStr(ctx, edata_obj, "type", JS_NewString(ctx, js_ivs_chunkType2name(js_ivs->audio_chunk_type)));
                        JS_SetPropertyStr(ctx, edata_obj, "time", JS_NewInt32(ctx, payload->time));
                        JS_SetPropertyStr(ctx, edata_obj, "length", JS_NewInt32(ctx, payload->length));
                        JS_SetPropertyStr(ctx, edata_obj, "samplerate", JS_NewInt32(ctx, payload->samplerate));
                        JS_SetPropertyStr(ctx, edata_obj, "channels", JS_NewInt32(ctx, payload->channels));
                        if(js_ivs->audio_chunk_type == IVS_CHUNK_TYPE_FILE) {
                            JS_SetPropertyStr(ctx, edata_obj, "file", JS_NewStringLen(ctx, payload->data, payload->data_len));
                        } else if(js_ivs->audio_chunk_type == IVS_CHUNK_TYPE_BUFFER) {
                            if(js_ivs->audio_chunk_encoding == IVS_CHUNK_ENCODING_B64) {
                               JS_SetPropertyStr(ctx, edata_obj, "buffer", JS_NewStringLen(ctx, payload->data, (payload->data_len > 1 ? (payload->data_len - 1) : payload->data_len)));
                            } else {
                                JS_SetPropertyStr(ctx, edata_obj, "buffer", JS_NewArrayBufferCopy(ctx, payload->data, (payload->data_len > 1 ? payload->data_len - 1 : payload->data_len)));
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    return ret_val;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_ivs_class = {
    CLASS_NAME,
    .finalizer = js_ivs_finalizer,
};

static const JSCFunctionListEntry js_ivs_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("isAudioCaptureActive", js_ivs_property_get, js_ivs_property_set, PROP_IS_AUDIO_CAPTURE_ACTIVE),
    JS_CGETSET_MAGIC_DEF("isVideoCaptureActive", js_ivs_property_get, js_ivs_property_set, PROP_IS_VIDEO_CAPTURE_ACTIVE),
    JS_CGETSET_MAGIC_DEF("isAudioCaptureOnPause", js_ivs_property_get, js_ivs_property_set, PROP_IS_AUDIO_CAPTURE_ON_PAUSE),
    JS_CGETSET_MAGIC_DEF("isVideoCaptureOnPause", js_ivs_property_get, js_ivs_property_set, PROP_IS_VIDEO_CAPTURE_ON_PAUSE),
    JS_CGETSET_MAGIC_DEF("audioChunkSec", js_ivs_property_get, js_ivs_property_set, PROP_CHUNK_SEC),
    JS_CGETSET_MAGIC_DEF("audioChunkType", js_ivs_property_get, js_ivs_property_set, PROP_CHUNK_TYPE),
    JS_CGETSET_MAGIC_DEF("audioChunkEncoding", js_ivs_property_get, js_ivs_property_set, PROP_CHUNK_ENCODING),
    JS_CGETSET_MAGIC_DEF("vadVoiceMs", js_ivs_property_get, js_ivs_property_set, PROP_VAD_VOICE_MS),
    JS_CGETSET_MAGIC_DEF("vadSilenceMs", js_ivs_property_get, js_ivs_property_set, PROP_VAD_SILENCE_MS),
    JS_CGETSET_MAGIC_DEF("vadThreshold", js_ivs_property_get, js_ivs_property_set, PROP_VAD_THRESHOLD),
    JS_CGETSET_MAGIC_DEF("vadDebugEnable", js_ivs_property_get, js_ivs_property_set, PROP_VAD_DEBUG),
    JS_CGETSET_MAGIC_DEF("cngEnable", js_ivs_property_get, js_ivs_property_set, PROP_CNG_ENABLED),
    JS_CGETSET_MAGIC_DEF("cngLevel", js_ivs_property_get, js_ivs_property_set, PROP_CNG_LEVEL),
    JS_CGETSET_MAGIC_DEF("dtmfMaxDigits", js_ivs_property_get, js_ivs_property_set, PROP_DTMF_MAX_DIGITS),
    JS_CGETSET_MAGIC_DEF("dtmMaxIdle", js_ivs_property_get, js_ivs_property_set, PROP_DTMF_MAX_IDLE),
    JS_CGETSET_MAGIC_DEF("silenceTimeout", js_ivs_property_get, js_ivs_property_set, PROP_SILENCE_TIMOUT),
    JS_CGETSET_MAGIC_DEF("sessionTimeout", js_ivs_property_get, js_ivs_property_set, PROP_SESSION_TIMOUT),
    JS_CGETSET_MAGIC_DEF("ttsEngine", js_ivs_property_get, js_ivs_property_set, PROP_TTS_ENGINE),
    JS_CGETSET_MAGIC_DEF("asrEngine", js_ivs_property_get, js_ivs_property_set, PROP_ASR_ENGINE),
    JS_CGETSET_MAGIC_DEF("language", js_ivs_property_get, js_ivs_property_set, PROP_LANGUAGE),
    JS_CFUNC_DEF("captureStart", 1, js_start_capture),
    JS_CFUNC_DEF("capturePause", 1, js_pause_capturing),
    JS_CFUNC_DEF("captureStop", 1, js_stop_capturing),
    JS_CFUNC_DEF("playback", 1, js_do_playback),
    JS_CFUNC_DEF("say", 1, js_do_say),
    JS_CFUNC_DEF("say2", 1, js_do_say_with_lang),
    JS_CFUNC_DEF("playbackStop", 1, js_playback_stop),
    JS_CFUNC_DEF("getEvent", 1, js_ivs_get_event),
    JS_CFUNC_DEF("timersStart", 1, js_timers_start),
    JS_CFUNC_DEF("timersStop", 1, js_timers_stop),
    JS_CFUNC_DEF("timerSetup", 1, js_timer_setup),  // seconds timers
    JS_CFUNC_DEF("timerCancel", 1, js_timer_cancel),
    JS_CFUNC_DEF("transcribe", 1, js_do_transcribe)
};

static void js_ivs_finalizer(JSRuntime *rt, JSValue val) {
    js_ivs_t *js_ivs = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));
    switch_memory_pool_t *pool = (js_ivs ? js_ivs->pool : NULL);
    uint8_t fl_wloop = false;

    if(!js_ivs) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "(js_ivs == NULL)\n");
        return;
    }

    js_ivs->fl_ready = false;

    switch_mutex_lock(js_ivs->mutex);
    fl_wloop = (js_ivs->wlock > 0);
    switch_mutex_unlock(js_ivs->mutex);

    if(js_ivs->wlock > 0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for unlock ('%d' locks) ...\n", js_ivs->wlock);
        while(fl_wloop) {
            switch_mutex_lock(js_ivs->mutex);
            fl_wloop = (js_ivs->wlock > 0);
            switch_mutex_unlock(js_ivs->mutex);
            switch_yield(100000);
        }
    }

    if(js_ivs->fl_fss_need_unlock && js_ivs->fs_session) {
        switch_core_session_rwunlock(js_ivs->fs_session);
    }

    if(js_ivs->fl_jss_need_unlock && js_ivs->js_session) {
        js_session_release(js_ivs->js_session);
    }

    js_ivs_events_queue_term(js_ivs);
    js_ivs_audio_queue_term(js_ivs);
    js_ivs_dtmf_queue_term(js_ivs);

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-ivs-finalizer: js_ivs=%p\n", js_ivs);
    js_free_rt(rt, js_ivs);

}

/*
 * new IVS(session || sessionId)
 */
static JSValue js_ivs_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_ivs_t *js_ivs = NULL;
    js_session_t *jss = NULL;
    switch_core_session_t *fs_session = NULL;
    switch_memory_pool_t *pool = NULL;
    const char *uuid = NULL;
    uint8_t fl_need_rw_unlock = false;
    uint8_t fl_jss_locked = false;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }
    if(QJS_IS_NULL(argv[0])) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: session");
        goto fail;
    }

    if(JS_IsObject(argv[0])) {
        jss = JS_GetOpaque2(ctx, argv[0], js_seesion_get_classid(ctx));
        if(jss) {
            fs_session = jss->session;
            fl_jss_locked = js_session_take(jss);
        } else {
            err = JS_ThrowTypeError(ctx, "Invalid session object");
            goto fail;
        }
    }
    if(jss == NULL) {
        uuid = JS_ToCString(ctx, argv[0]);
        if(!zstr(uuid)) {
            fs_session = switch_core_session_locate(uuid);
            fl_need_rw_unlock = true;
        } else {
            err = JS_ThrowTypeError(ctx, "Session not found");
            goto fail;
        }
    }

    js_ivs = js_mallocz(ctx, sizeof(js_ivs_t));
    if(!js_ivs) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }
    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
        goto fail;
    }

    switch_mutex_init(&js_ivs->mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&js_ivs->mutex_timers, SWITCH_MUTEX_NESTED, pool);
    switch_queue_create(&js_ivs->eventsq, IVS_EVENTS_QUEUE_SIZE, pool);
    switch_queue_create(&js_ivs->audioq, IVS_EVENTS_QUEUE_SIZE, pool);
    switch_queue_create(&js_ivs->dtmfq, IVS_DTMF_QUEUE_SIZE, pool);

    js_ivs->pool = pool;
    js_ivs->js_session = jss;
    js_ivs->fs_session = fs_session;
    js_ivs->fl_fss_need_unlock = fl_need_rw_unlock;
    js_ivs->fl_jss_need_unlock = fl_jss_locked;

    js_ivs->cng_lvl = 500;
    js_ivs->dtmf_idle_sec = 1;
    js_ivs->dtmf_max_digits = 1;
    js_ivs->vad_voice_ms = 200;
    js_ivs->vad_silence_ms = 350;
    js_ivs->vad_threshold = 200;
    js_ivs->audio_chunk_sec = 15;
    js_ivs->silence_timeout_sec = 0;
    js_ivs->audio_chunk_type = IVS_CHUNK_TYPE_BUFFER;
    js_ivs->audio_chunk_encoding = IVS_CHUNK_ENCODING_RAW;
    js_ivs->origin = switch_core_sprintf(pool, "%p", js_ivs);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_ivs_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    js_ivs->fl_ready = true;
    JS_SetOpaque(obj, js_ivs);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-ivs-constructor: js_ivs=%p, js_session=%p, fs_session=%p\n", js_ivs, jss, fs_session);

    return obj;
fail:
    if(js_ivs) {
        if(js_ivs->eventsq) {
            switch_queue_term(js_ivs->eventsq);
        }
        if(js_ivs->audioq) {
            switch_queue_term(js_ivs->audioq);
        }
        if(js_ivs->dtmfq) {
            switch_queue_term(js_ivs->dtmfq);
        }
        js_free(ctx, js_ivs);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(fs_session && fl_need_rw_unlock) {
        switch_core_session_rwunlock(fs_session);
    }
    if(jss && fl_jss_locked) {
        js_session_release(jss);
    }

    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, uuid);

    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_ivs_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_ivs_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_ivs_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_ivs_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_ivs_proto_funcs, ARRAY_SIZE(js_ivs_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_ivs_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

