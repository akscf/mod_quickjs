/**
 * (C)2021-2025 aks
 * https://github.com/akscf/
 *
 **/
#include "js_session.h"
#include "js_file.h"
#include "js_event.h"
#include "js_codec.h"
#include "js_eventhandler.h"
#include "js_filehandle.h"

#define CLASS_NAME                          "Session"
#define PROP_NAME                           0
#define PROP_UUID                           1
#define PROP_STATE                          2
#define PROP_CAUSE                          3
#define PROP_CAUSECODE                      4
#define PROP_CALLER_ID_NAME                 5
#define PROP_CALLER_ID_NUMBER               6
#define PROP_PROFILE_DIALPLAN               7
#define PROP_PROFILE_DESTINATION            8
#define PROP_RCODEC_NAME                    9
#define PROP_WCODEC_NAME                    10
#define PROP_SAMPLERATE                     11
#define PROP_CHANNELS                       12
#define PROP_PTIME                          13
#define PROP_IS_READY                       14
#define PROP_IS_ANSWERED                    15
#define PROP_IS_MEDIA_READY                 16
#define PROP_ENCODED_FRAME_SIZE             17
#define PROP_DECODED_FRAME_SIZE             18
#define PROP_TTS_ENGINE                     19
#define PROP_ASR_ENGINE                     20
#define PROP_LANGUAGE                       21
#define PROP_BG_STREAMS                     22
#define PROP_AUTO_HANGUP                    23

#define SESSION_SANITY_CHECK() if (!jss || !jss->session) { \
           return JS_ThrowTypeError(ctx, "Session is not initialized"); \
        }

#define CHANNEL_SANITY_CHECK() do { \
           if(!switch_channel_ready(channel)) { \
                return JS_ThrowTypeError(ctx, "Channel is not ready"); \
           } \
           if(!((switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA)))) { \
                return JS_ThrowTypeError(ctx, "Session is not answered!"); \
           } \
        } while(0)

#define CHANNEL_SANITY_CHECK_ANSWER() do { \
         if (!switch_channel_ready(channel)) { \
            return JS_ThrowTypeError(ctx, "Session is not active!"); \
         }                                                                                                                               \
        } while(0)

#define CHANNEL_MEDIA_SANITY_CHECK() do { \
        if(!switch_channel_media_ready(channel)) { \
            return JS_ThrowTypeError(ctx, "Session is not in media mode!"); \
        } \
    } while(0)

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    js_session_t    *jss;
    JSValue         fh_obj;
    JSValue         jss_obj;
    JSValue         arg;
    JSValue         function;
    js_session_t    *jss_a;
    js_session_t    *jss_b;
    JSValue         jss_a_obj;
    JSValue         jss_b_obj;
} input_callback_state_t;


static void js_session_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static switch_status_t xxx_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
static switch_status_t sys_session_hangup_hook(switch_core_session_t *session);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_session_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_caller_profile_t *caller_profile = NULL;
    switch_channel_t *channel = NULL;

     /* when originate failed */
    if(jss && jss->session == NULL) {
        switch(magic) {
            case PROP_IS_READY:
            case PROP_IS_ANSWERED:
            case PROP_IS_MEDIA_READY:
                return JS_FALSE;
            case PROP_CAUSE: {
                if(jss) { return JS_NewString(ctx, switch_channel_cause2str(jss->originate_fail_code) ); }
                return JS_UNDEFINED;
            }
            case PROP_CAUSECODE: {
                if(jss) { return JS_NewInt32(ctx, jss->originate_fail_code); }
                return JS_UNDEFINED;
            }
            default:
                return JS_UNDEFINED;
        }
    }

    SESSION_SANITY_CHECK();

    channel = switch_core_session_get_channel(jss->session);
    caller_profile = switch_channel_get_caller_profile(channel);

    switch(magic) {
        case PROP_NAME: {
            return JS_NewString(ctx, switch_channel_get_name(channel));
        }
        case PROP_UUID: {
            return JS_NewString(ctx, switch_channel_get_uuid(channel));
        }
        case PROP_STATE: {
            return JS_NewString(ctx, switch_channel_state_name(switch_channel_get_state(channel)) );
        }
        case PROP_CAUSE: {
            return JS_NewString(ctx, switch_channel_cause2str(switch_channel_get_cause(channel)) );
        }
        case PROP_CAUSECODE: {
            return JS_NewInt32(ctx, switch_channel_get_cause(channel));
        }
        case PROP_CALLER_ID_NAME: {
            if(caller_profile) { return JS_NewString(ctx, caller_profile->caller_id_name); }
            return JS_UNDEFINED;
        }
        case PROP_CALLER_ID_NUMBER: {
            if(caller_profile) { return JS_NewString(ctx, caller_profile->caller_id_number); }
            return JS_UNDEFINED;
        }
        case PROP_PROFILE_DIALPLAN: {
            if(caller_profile) { return JS_NewString(ctx, caller_profile->dialplan); }
            return JS_UNDEFINED;
        }
        case PROP_PROFILE_DESTINATION: {
            if(caller_profile) { return JS_NewString(ctx, caller_profile->destination_number); }
            return JS_UNDEFINED;
        }
        case PROP_SAMPLERATE: {
            return JS_NewInt32(ctx, jss->samplerate);
        }
        case PROP_CHANNELS: {
            return JS_NewInt32(ctx, jss->channels);
        }
        case PROP_PTIME: {
            return JS_NewInt32(ctx, jss->ptime);
        }
        case PROP_RCODEC_NAME: {
            const char *name = switch_channel_get_variable(channel, "read_codec");
            if(!zstr(name)) { return JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }
        case PROP_WCODEC_NAME: {
            const char *name = switch_channel_get_variable(channel, "write_codec");
            if(!zstr(name)) { return JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }
        case PROP_IS_READY: {
            return (switch_channel_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_ANSWERED: {
            return (switch_channel_test_flag(switch_core_session_get_channel(jss->session), CF_ANSWERED) ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_MEDIA_READY: {
            return (switch_channel_media_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
        }
        case PROP_ENCODED_FRAME_SIZE: {
            return JS_NewInt32(ctx, jss->encoded_frame_size);
        }
        case PROP_DECODED_FRAME_SIZE: {
            return JS_NewInt32(ctx, jss->decoded_frame_size);
        }
        case PROP_TTS_ENGINE: {
            const char *name = switch_channel_get_variable(channel, "tts_engine");
            if(!zstr(name)) { return JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }
        case PROP_ASR_ENGINE: {
            const char *name = switch_channel_get_variable(channel, "asr_engine");
            if(!zstr(name)) { return JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }
        case PROP_LANGUAGE: {
            const char *name = switch_channel_get_variable(channel, "language");
            if(!zstr(name)) { return JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }
        case PROP_BG_STREAMS: {
            return JS_NewInt32(ctx, jss->bg_streams);
        }
        case PROP_AUTO_HANGUP: {
            return (jss->fl_hup_auto ? JS_TRUE : JS_FALSE);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_session_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();

    channel = switch_core_session_get_channel(jss->session);

    switch(magic) {
        case PROP_TTS_ENGINE: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            const char *str = JS_ToCString(ctx, val);
            switch_channel_set_variable_var_check(channel, "tts_engine", str, false);
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_ASR_ENGINE: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            const char *str = JS_ToCString(ctx, val);
            switch_channel_set_variable_var_check(channel, "asr_engine", str, false);
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_LANGUAGE: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            const char *str = JS_ToCString(ctx, val);
            switch_channel_set_variable_var_check(channel, "language", str, false);
            switch_channel_set_variable_var_check(channel, "tts_voice", str, false); // for say via switch_ivr_play_file
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_AUTO_HANGUP: {
            jss->fl_hup_auto = JS_ToBool(ctx, val);
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSValue js_session_set_hangup_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(!argc) {
        return JS_ThrowTypeError(ctx, "Invalid argument: callback function");
    }

    if(JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        jss->fl_hup_hook = false;

    } else if(JS_IsFunction(ctx, argv[0])) {
        jss->on_hangup = argv[0];
        jss->fl_hup_hook = true;

        switch_channel_set_private(channel, "jss", jss);
        switch_core_event_hook_add_state_change(jss->session, sys_session_hangup_hook);
    }

    return JS_TRUE;
}

// speak(text, [eventsHandler, handlerData])
static JSValue  js_session_speak(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *text = NULL;
    const char *tts_engine = NULL;
    const char *tts_language = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "speak(text, [eventsHandler, handlerData])");
    }

    tts_engine = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "tts_engine");
    if(zstr(tts_engine)) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: tts_engine");
    }

    tts_language = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "language");
    if(zstr(tts_language)) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: language");
    }

    text = JS_ToCString(ctx, argv[0]);
    if(zstr(text)) {
        return JS_ThrowTypeError(ctx, "Invalid agument: text");
    }

    if(argc > 1 && JS_IsFunction(ctx, argv[1])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
        cb_state.function = argv[1];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_speak_text(jss->session, tts_engine, tts_language, text, &args);

    JS_FreeCString(ctx, text);
    return JS_TRUE;
}

// speakEx(engine, language, text, [eventsHandler, handlerData])
static JSValue  js_session_speak_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *text = NULL;
    const char *tts_engine = NULL;
    const char *ch_tts_engine = NULL;
    const char *tts_language = NULL;
    const char *ch_tts_language = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };

    SESSION_SANITY_CHECK();

    if(!argc) {
        return JS_ThrowTypeError(ctx, "speakEx(ttsEngine, language, text, [eventsHandler, handlerData])");
    }

    if(QJS_IS_NULL(argv[0])) {
        ch_tts_engine = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "tts_engine");
        if(zstr(ch_tts_engine)) {
            return JS_ThrowTypeError(ctx, "Missing argument: tts_engine");
        }
    }
    if(QJS_IS_NULL(argv[1])) {
        ch_tts_language = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "language");
        if(zstr(ch_tts_language)) {
            return JS_ThrowTypeError(ctx, "Invalid argument: language");
        }
    }
    if(QJS_IS_NULL(argv[2])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: text");
    }

    tts_engine = JS_ToCString(ctx, argv[0]);
    tts_language = JS_ToCString(ctx, argv[1]);
    text = JS_ToCString(ctx, argv[2]);

    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_speak_text(jss->session, (tts_engine ? tts_engine : ch_tts_engine), (tts_language ? tts_language : ch_tts_language), text, &args);

    JS_FreeCString(ctx, tts_engine);
    JS_FreeCString(ctx, tts_language);
    JS_FreeCString(ctx, text);

    return JS_TRUE;
}

// sayPhrase(phraseName, phraseData, phraseLang, eventsHandler, handlerData)
static JSValue js_session_say_phrase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *phrase_name = NULL, *phrase_data = NULL, *phrase_lang = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };

    SESSION_SANITY_CHECK();

    if(argc > 0) {
        if(QJS_IS_NULL(argv[0])) { return JS_FALSE; }
        phrase_name = JS_ToCString(ctx, argv[0]);
    }
    if(argc > 1) {
        phrase_data = (QJS_IS_NULL(argv[1]) ? NULL : JS_ToCString(ctx, argv[1]));
    }
    if(argc > 2) {
        phrase_lang = (QJS_IS_NULL(argv[2]) ? NULL : JS_ToCString(ctx, argv[2]));
    }
    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_phrase_macro(jss->session, phrase_name, phrase_data, phrase_lang, &args);

    JS_FreeCString(ctx, phrase_name);
    JS_FreeCString(ctx, phrase_data);
    JS_FreeCString(ctx, phrase_lang);

    return JS_TRUE;
}

// playback(fileName, [skipSmps, eventsHandler, handlerData])
static JSValue js_session_playback(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    char *file_obj_fname = NULL;
    const char *file_name = NULL;
    const char *prebuf = NULL;
    char posbuf[64] = "";
    input_callback_state_t cb_state = { 0 };
    switch_file_handle_t fh = { 0 };
    switch_input_args_t args = { 0 };
    js_file_t *js_file = NULL;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "playback(fileName, [skipSmps, eventsHandler, handlerData])");
    }

    js_file = JS_GetOpaque(argv[0], js_file_get_classid(ctx));
    if(js_file) {
        file_obj_fname = strdup(js_file->path);
    } else {
        file_name = JS_ToCString(ctx, argv[0]);
    }

    if(argc > 1) {
        uint32_t val;
        JS_ToUint32(ctx, &val, argv[1]);
        fh.samples = val;
    }

    if(argc > 2 && JS_IsFunction(ctx, argv[2])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 3 ? argv[3] : JS_UNDEFINED);
        cb_state.function = argv[2];
        cb_state.fh_obj = js_file_handle_object_create(ctx, &fh, jss->session);

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    if((prebuf = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "stream_prebuffer"))) {
        int val = atoi(prebuf);
        if(val > 0) { fh.prebuf = val; }
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_play_file(jss->session, &fh, (file_name ? file_name : file_obj_fname), &args);

    JS_FreeValue(ctx, cb_state.fh_obj);
    JS_FreeCString(ctx, file_name);
    switch_safe_free(file_obj_fname);

    switch_snprintf(posbuf, sizeof(posbuf), "%u", fh.offset_pos);
    switch_channel_set_variable(switch_core_session_get_channel(jss->session), "last_file_position", posbuf);

    return JS_TRUE;
}

// playbackStop()
static JSValue js_session_playback_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    if(jss->bg_streams) {
        js_session_bgs_stream_stop(jss);
    } else {
        switch_channel_set_flag(switch_core_session_get_channel(jss->session), CF_BREAK);
    }

    return JS_TRUE;
}

// bgPlaybackStart(fileName)
static JSValue js_session_bg_playback_start(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    char *js_obj_fname = NULL;
    const char *fname = NULL;
    js_file_t *js_file = NULL;
    JSValue result = JS_FALSE;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "bgPlaybackStart(fileName)");
    }

    js_file = JS_GetOpaque(argv[0], js_file_get_classid(ctx));
    if(js_file) {
        js_obj_fname = js_file->path;
    } else {
        fname = JS_ToCString(ctx, argv[0]);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    if(js_session_bgs_stream_start(jss, fname ? fname : js_obj_fname) == SWITCH_STATUS_SUCCESS) {
        result = JS_TRUE;
    }

    JS_FreeCString(ctx, fname);
    return result;
}

// bgPlaybackCtl(pause|resume|speed|volume,[+|-|n])
static JSValue js_session_bg_playback_ctl(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *action = NULL;
    JSValue result = JS_FALSE;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "bgPlaybackCtl(pause|resume|speed|volume,[+|-|n])");
    }

    action = JS_ToCString(ctx, argv[0]);

    if(!strcasecmp(action, "pause")) {
        switch_mutex_lock(jss->mutex);
        if(jss->bg_stream_fh) switch_set_flag(jss->bg_stream_fh, SWITCH_FILE_PAUSE);
        switch_mutex_unlock(jss->mutex);
    } else if(!strcasecmp(action, "resume")) {
        switch_mutex_lock(jss->mutex);
        if(jss->bg_stream_fh) switch_clear_flag(jss->bg_stream_fh, SWITCH_FILE_PAUSE);
        switch_mutex_unlock(jss->mutex);
    } else if(!strcasecmp(action, "speed")) {
        if(argc > 1) {
            if(JS_IsNumber(argv[1])) {
                int32_t ival = 0;
                JS_ToUint32(ctx, &ival, argv[1]);
                switch_mutex_lock(jss->mutex);
                if(jss->bg_stream_fh) jss->bg_stream_fh->speed = ival;
                switch_mutex_unlock(jss->mutex);
            } else {
                const char *sval = JS_ToCString(ctx, argv[1]);
                if(sval[0] == '+' || sval[0] == '-') {
                    int step = atoi(sval);
                    switch_mutex_lock(jss->mutex);
                    if(jss->bg_stream_fh) jss->bg_stream_fh->speed += (!step ? 1 : step);
                    switch_mutex_unlock(jss->mutex);
                } else {
                    switch_mutex_lock(jss->mutex);
                    if(jss->bg_stream_fh) jss->bg_stream_fh->speed += atoi(sval);
                    switch_mutex_unlock(jss->mutex);
                }
                JS_FreeCString(ctx, sval);
            }
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing arg2\n");
        }
    } else if(!strcasecmp(action, "volume")) {
        if(argc > 1) {
            if(JS_IsNumber(argv[1])) {
                int32_t ival = 0;
                JS_ToUint32(ctx, &ival, argv[1]);
                switch_mutex_lock(jss->mutex);
                if(jss->bg_stream_fh) jss->bg_stream_fh->vol = ival;
                switch_mutex_unlock(jss->mutex);
            } else {
                const char *sval = JS_ToCString(ctx, argv[1]);
                if(sval[0] == '+' || sval[0] == '-') {
                    int step = atoi(sval);
                    switch_mutex_lock(jss->mutex);
                    if(jss->bg_stream_fh) jss->bg_stream_fh->vol += (!step ? 1 : step);
                    switch_mutex_unlock(jss->mutex);
                } else {
                    switch_mutex_lock(jss->mutex);
                    if(jss->bg_stream_fh) jss->bg_stream_fh->vol += atoi(sval);
                    switch_mutex_unlock(jss->mutex);
                }
                JS_FreeCString(ctx, sval);
            }
        } else {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing arg2\n");
        }
    }

    JS_FreeCString(ctx, action);
    return result;
}

// bgPlaybackStop()
static JSValue js_session_bg_playback_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    if(js_session_bgs_stream_stop(jss) == SWITCH_STATUS_SUCCESS) {
        return JS_TRUE;
    }

    return JS_FALSE;
}

// playAndGetDigits(min_digits, max_digits, max_tries ,timeout, terminators, audio_file, bad_audio_file, [digits_regex, var_name, digit_timeout, transfer_on_failure])
static JSValue js_session_play_and_get_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    char buf[513] = { 0 };
    uint32_t buf_len = (sizeof(buf) - 1);
    uint32_t min_digits = 0, max_digits = 0, max_tries = 0, timeout = 0, digit_timeout = 0;
    const char *terminators = NULL, *audio_file = NULL, *bad_audio_file = NULL, *digits_regex = NULL, *var_name = NULL, *transfer_on_failure = NULL;
    char *terminators_ref = NULL;
    JSValue result = JS_UNDEFINED;

    SESSION_SANITY_CHECK();

    if(argc < 7) {
        return JS_ThrowTypeError(ctx, "playAndGetDigits(min_digits, max_digits, max_tries ,timeout, terminators, audio_file, bad_audio_file, [digits_regex, var_name, digit_timeout, transfer_on_failure])");
    }

    JS_ToUint32(ctx, &min_digits, argv[0]);
    JS_ToUint32(ctx, &max_digits, argv[1]);
    JS_ToUint32(ctx, &max_tries, argv[2]);
    JS_ToUint32(ctx, &timeout, argv[3]);

    terminators = (QJS_IS_NULL(argv[4]) ? NULL : JS_ToCString(ctx, argv[4]));
    audio_file = (QJS_IS_NULL(argv[5]) ? NULL : JS_ToCString(ctx, argv[5]));
    bad_audio_file = (QJS_IS_NULL(argv[6]) ? NULL : JS_ToCString(ctx, argv[6]));

    if(argc > 7) {
        digits_regex = (QJS_IS_NULL(argv[7]) ? NULL : JS_ToCString(ctx, argv[7]));
    }
    if(argc > 8) {
        var_name = (QJS_IS_NULL(argv[8]) ? NULL : JS_ToCString(ctx, argv[8]));
    }
    if(argc > 9) {
        JS_ToUint32(ctx, &digit_timeout, argv[9]);
    }
    if(argc > 10) {
        transfer_on_failure = (QJS_IS_NULL(argv[10]) ? NULL : JS_ToCString(ctx, argv[10]));
    }

    if(max_digits < min_digits) {
        max_digits = min_digits;
    }

    if(timeout < 1000) {
        timeout = 1000;
    }

    terminators_ref = (zstr(terminators_ref) ? "#" : (char *)terminators);

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_play_and_get_digits(jss->session, min_digits, max_digits, max_tries, timeout, terminators_ref, audio_file, bad_audio_file, var_name, buf, buf_len, digits_regex, digit_timeout, transfer_on_failure);
    result = JS_NewString(ctx, (char *) buf);

    JS_FreeCString(ctx, terminators);
    JS_FreeCString(ctx, audio_file);
    JS_FreeCString(ctx, bad_audio_file);
    JS_FreeCString(ctx, digits_regex);
    JS_FreeCString(ctx, var_name);
    JS_FreeCString(ctx, transfer_on_failure);

    return result;
}

// playAndDetectSpeech(fileToPlay, [timeout, asrExtraParam, eventsHandler, handlerData])
static JSValue js_session_play_and_detect_speech(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_status_t status = 0;
    uint32_t timeout = 0;
    char *stt_result = NULL;
    char *asr_extra_params = NULL;
    const char *file_name = NULL;
    const char *extra_params = NULL;
    const char *asr_engine_name = NULL;
    JSValue result = JS_UNDEFINED;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "playAndDetectSpeech(fileToPlay, [timeout, asrExtraParam, eventsHandler, handlerData])");
    }

    asr_engine_name = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "asr_engine");
    if(zstr(asr_engine_name)) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: asr_engine");
    }

    file_name = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        JS_ToUint32(ctx, &timeout, argv[1]);
    }
    if(argc > 2) {
        extra_params = JS_ToCString(ctx, argv[2]);
    }
    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    if(extra_params) {
        asr_extra_params = switch_mprintf("{limit=%d, %s}", timeout, extra_params);
    } else {
        asr_extra_params = switch_mprintf("{limit=%d}", timeout);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    status = switch_ivr_play_and_detect_speech_ex(jss->session, file_name, asr_engine_name, asr_extra_params, &stt_result, timeout, &args);

    if(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_FALSE) {
        result = zstr(stt_result) ? JS_UNDEFINED : JS_NewString(ctx, stt_result);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_ivr_play_and_detect_speech_ex() failed (status=%i)\n", status);
        result = JS_NewString(ctx, "[engine failure]");
    }

    JS_FreeCString(ctx, file_name);
    JS_FreeCString(ctx, extra_params);
    switch_safe_free(asr_extra_params);

    return result;
}

// sayAndDetectSpeech(textToSpeech, [timeout, asrExtraParam, eventsHandler, handlerData])
static JSValue js_session_say_and_detect_speech(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_status_t status = 0;
    uint32_t timeout = 0;
    char *stt_result = NULL;
    char *stt_source = NULL;
    char *asr_extra_params = NULL;
    const char *text = NULL;
    const char *extra_params = NULL;
    const char *asr_engine_name = NULL;
    JSValue result = JS_UNDEFINED;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "sayAndDetectSpeech(textToSpeech, [timeout, asrExtraParam, eventsHandler, handlerData])");
    }

    asr_engine_name = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "asr_engine");
    if(zstr(asr_engine_name)) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: asr_engine");
    }
    if(zstr(switch_channel_get_variable(switch_core_session_get_channel(jss->session), "tts_engine"))) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: tts_engine");
    }

    if(argc > 1) {
        JS_ToUint32(ctx, &timeout, argv[1]);
    }
    if(argc > 2) {
        extra_params = JS_ToCString(ctx, argv[2]);
    }
    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    text = JS_ToCString(ctx, argv[0]);
    stt_source = switch_mprintf("say:%s", text);

    if(extra_params) {
        asr_extra_params = switch_mprintf("{limit=%d, %s}", timeout, extra_params);
    } else {
        asr_extra_params = switch_mprintf("{limit=%d}", timeout);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    status = switch_ivr_play_and_detect_speech_ex(jss->session, stt_source, asr_engine_name, asr_extra_params, &stt_result, timeout, &args);

    if(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_FALSE) {
        result = zstr(stt_result) ? JS_UNDEFINED : JS_NewString(ctx, stt_result);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_ivr_play_and_detect_speech_ex() failed (status=%i)\n", status);
        result = JS_NewString(ctx, "[engine failure]");
    }

    JS_FreeCString(ctx, text);
    JS_FreeCString(ctx, extra_params);
    switch_safe_free(asr_extra_params);
    switch_safe_free(stt_source);

    return result;
}

// detectSpeech([timeout, asrExtraParam, eventsHandler, handlerData])
static JSValue js_session_detect_speech(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_status_t status = 0;
    uint32_t timeout = 0;
    char *stt_result = NULL;
    char *asr_extra_params = NULL;
    const char *extra_params = NULL;
    const char *asr_engine_name = NULL;
    JSValue result = JS_UNDEFINED;

    SESSION_SANITY_CHECK();

    asr_engine_name = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "asr_engine");
    if(zstr(asr_engine_name)) {
        return JS_ThrowTypeError(ctx, "Missing channel variable: asr_engine");
    }

    if(argc >= 1) {
        JS_ToUint32(ctx, &timeout, argv[0]);
    }
    if(argc > 1) {
        extra_params = JS_ToCString(ctx, argv[1]);
    }
    if(argc > 2 && JS_IsFunction(ctx, argv[2])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 3 ? argv[3] : JS_UNDEFINED);
        cb_state.function = argv[2];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    if(extra_params) {
        asr_extra_params = switch_mprintf("{limit=%d, %s}", timeout, extra_params);
    } else {
        asr_extra_params = switch_mprintf("{limit=%d}", timeout);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    status = switch_ivr_play_and_detect_speech_ex(jss->session, NULL, asr_engine_name, asr_extra_params, &stt_result, timeout, &args);

    if(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_FALSE) {
        result = zstr(stt_result) ? JS_UNDEFINED : JS_NewString(ctx, stt_result);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_ivr_play_and_detect_speech_ex() failed (status=%i)\n", status);
        result = JS_NewString(ctx, "[engine failure]");
    }

    JS_FreeCString(ctx, extra_params);
    switch_safe_free(asr_extra_params);

    return result;
}

// detectSpeechEx(asrEngine, [timeout, asrExtraParam, eventsHandler, handlerData])
static JSValue js_session_detect_speech_ex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_status_t status = 0;
    uint32_t timeout = 0;
    char *stt_result = NULL;
    char *asr_extra_params = NULL;
    const char *extra_params = NULL;
    const char *asr_engine_name = NULL;
    const char *ch_asr_engine_name = NULL;
    JSValue result = JS_UNDEFINED;

    SESSION_SANITY_CHECK();

    if(!argc) {
        return JS_ThrowTypeError(ctx, "detectSpeechEx(asrEngine, [timeout, asrExtraParam, eventsHandler, handlerData])");
    }

    if(!QJS_IS_NULL(argv[0])) {
        asr_engine_name = JS_ToCString(ctx, argv[0]);
    } else {
        ch_asr_engine_name = switch_channel_get_variable(switch_core_session_get_channel(jss->session), "asr_engine");
    }
    if(zstr(asr_engine_name) && zstr(ch_asr_engine_name)) {
        return JS_ThrowTypeError(ctx, "Missing argument: asrEngine");
    }

    if(argc > 1) {
        JS_ToUint32(ctx, &timeout, argv[1]);
    }
    if(argc > 2) {
        extra_params = JS_ToCString(ctx, argv[2]);
    }
    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    if(extra_params) {
        asr_extra_params = switch_mprintf("{limit=%d, %s}", timeout, extra_params);
    } else {
        asr_extra_params = switch_mprintf("{limit=%d}", timeout);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    status = switch_ivr_play_and_detect_speech_ex(jss->session, NULL, asr_engine_name ? asr_engine_name : ch_asr_engine_name, asr_extra_params, &stt_result, timeout, &args);

    if(status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_FALSE) {
        result = zstr(stt_result) ? JS_UNDEFINED : JS_NewString(ctx, stt_result);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_ivr_play_and_detect_speech_ex() failed (status=%i)\n", status);
        result = JS_NewString(ctx, "[engine failure]");
    }

    JS_FreeCString(ctx, asr_engine_name);
    JS_FreeCString(ctx, extra_params);
    switch_safe_free(asr_extra_params);

    return result;
}

// recordFile(fileName, [limitSec, threshold, silinceHits, eventsHandler, handlerData])
static JSValue js_session_record_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    char *file_obj_fname = NULL;
    const char *file_name = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_file_handle_t fh = { 0 };
    switch_input_args_t args = { 0 };
    js_file_t *js_file = NULL;
    uint32_t limit = 0;

    SESSION_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "recordFile(fileName, [limitSec, threshold, silinceHits, eventsHandler, handlerData])");
    }

    js_file = JS_GetOpaque(argv[0], js_file_get_classid(ctx));
    if(js_file) {
        file_obj_fname = strdup(js_file->path);
    } else {
        file_name = JS_ToCString(ctx, argv[0]);
    }

    if(argc > 1) {
        JS_ToUint32(ctx, &limit, argv[1]);
    }
    if(argc > 2) {
        uint32_t val;
        JS_ToUint32(ctx, &val, argv[2]);
        fh.thresh = val;
    }
    if(argc > 3) {
        uint32_t val;
        JS_ToUint32(ctx, &val, argv[3]);
        fh.silence_hits = val;
    }
    if(argc > 4 && JS_IsFunction(ctx, argv[4])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss;
        cb_state.arg = (argc > 5 ? argv[5] : JS_UNDEFINED);
        cb_state.function = argv[4];
        cb_state.fh_obj = js_file_handle_object_create(ctx, &fh, jss->session);

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_record_file(jss->session, &fh, (file_name ? file_name : file_obj_fname), &args, limit);

    JS_FreeValue(ctx, cb_state.fh_obj);
    JS_FreeCString(ctx, file_name);
    switch_safe_free(file_obj_fname);

    return JS_TRUE;
}

// collectInput(dtmfCallback, [udata, absTimeout, digitTimeout])
static JSValue js_session_collect_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    uint32_t abs_timeout = 0, digit_timeout = 0;

    SESSION_SANITY_CHECK();

    if(argc < 1 || QJS_IS_NULL(argv[0]) || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "collectInput(dtmfCallback, [udata, absTimeout, digitTimeout])");
    }

    memset(&cb_state, 0, sizeof(cb_state));
    cb_state.jss_obj = this_val;
    cb_state.jss = jss;
    cb_state.arg = (argc > 1 ? argv[1] : JS_UNDEFINED);
    cb_state.function = argv[0];

    args.input_callback = xxx_input_callback;
    args.buf = &cb_state;
    args.buflen = sizeof(cb_state);

    if(argc > 2) {
        JS_ToUint32(ctx, &abs_timeout, argv[2]);
    }
    if(argc > 3) {
        JS_ToUint32(ctx, &digit_timeout, argv[3]);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_collect_digits_callback(jss->session, &args, digit_timeout, abs_timeout);

    return JS_TRUE;
}

static JSValue js_session_flush_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_event_t *event;

    SESSION_SANITY_CHECK();
    while(switch_core_session_dequeue_event(jss->session, &event, false) == SWITCH_STATUS_SUCCESS) {
        switch_event_destroy(&event);
    }

    return JS_TRUE;
}

static JSValue js_session_flush_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));

    return JS_TRUE;
}

// setVariable(name, value)
static JSValue js_session_set_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *var, *val;

    SESSION_SANITY_CHECK();

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "setVariable(name, value)");
    }

    var = JS_ToCString(ctx, argv[0]);
    val = JS_ToCString(ctx, argv[1]);

    switch_channel_set_variable_var_check(switch_core_session_get_channel(jss->session), var, val, false);

    JS_FreeCString(ctx, var);
    JS_FreeCString(ctx, val);

    return JS_TRUE;
}

// getVariable(name)
static JSValue js_session_get_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *var, *val;

    SESSION_SANITY_CHECK();

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "getVariable(name)");
    }

    var = JS_ToCString(ctx, argv[0]);
    val = switch_channel_get_variable(switch_core_session_get_channel(jss->session), var);
    JS_FreeCString(ctx, var);

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

// setChanFlag(name, true|false)
static JSValue js_session_set_chan_flag(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    JSValue ret = JS_FALSE;
    switch_channel_flag_t flag = CF_FLAG_MAX;
    uint32_t val = 0;
    const char *name;

    SESSION_SANITY_CHECK();

    if(argc < 1 || QJS_IS_NULL(argv[0]) || !JS_IsBool(argv[1])) {
        return JS_ThrowTypeError(ctx, "setChanFlag(name, true|false)");
    }

    name = JS_ToCString(ctx, argv[0]);
    val = JS_ToBool(ctx, argv[1]);

    if(!strcasecmp(name, "CF_BREAK")) {
        flag = CF_BREAK;
    } else if(!strcasecmp(name, "CF_NO_RECOVER")) {
        flag = CF_NO_RECOVER;
    } else if(!strcasecmp(name, "CF_AUDIO_PAUSE_READ")) {
        flag = CF_AUDIO_PAUSE_READ;
    } else if(!strcasecmp(name, "CF_AUDIO_PAUSE_WRITE")) {
        flag = CF_AUDIO_PAUSE_WRITE;
    } else if(!strcasecmp(name, "CF_VIDEO_PAUSE_READ")) {
        flag = CF_VIDEO_PAUSE_WRITE;
    } else if(!strcasecmp(name, "CF_VIDEO_PAUSE_WRITE")) {
        flag = CF_VIDEO_BREAK;
    } else if(!strcasecmp(name, "CF_VIDEO_BREAK")) {
        flag = CF_VIDEO_BREAK;
    } else if(!strcasecmp(name, "CF_VIDEO_ECHO")) {
        flag = CF_VIDEO_ECHO;
    } else if(!strcasecmp(name, "CF_VIDEO_BLANK")) {
        flag = CF_VIDEO_BLANK;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported flag: %s\n", name);
    }

    if(flag != CF_FLAG_MAX) {
        if(val) {
            switch_channel_set_flag(switch_core_session_get_channel(jss->session), flag);
        } else {
            switch_channel_clear_flag(switch_core_session_get_channel(jss->session), flag);
        }
        ret = JS_TRUE;
    }

    JS_FreeCString(ctx, name);
    return ret;
}

// getChanFlag(name)
static JSValue js_session_get_chan_flag(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    JSValue ret = JS_FALSE;
    switch_channel_flag_t flag = CF_FLAG_MAX;
    const char *name;

    SESSION_SANITY_CHECK();

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "getChanFlag(name)");
    }

    name = JS_ToCString(ctx, argv[0]);
    if(!strcasecmp(name, "CF_BREAK")) {
        flag = CF_BREAK;
    } else if(!strcasecmp(name, "CF_NO_RECOVER")) {
        flag = CF_NO_RECOVER;
    } else if(!strcasecmp(name, "CF_AUDIO_PAUSE_READ")) {
        flag = CF_AUDIO_PAUSE_READ;
    } else if(!strcasecmp(name, "CF_AUDIO_PAUSE_WRITE")) {
        flag = CF_AUDIO_PAUSE_WRITE;
    } else if(!strcasecmp(name, "CF_VIDEO_PAUSE_READ")) {
        flag = CF_VIDEO_PAUSE_WRITE;
    } else if(!strcasecmp(name, "CF_VIDEO_PAUSE_WRITE")) {
        flag = CF_VIDEO_BREAK;
    } else if(!strcasecmp(name, "CF_VIDEO_BREAK")) {
        flag = CF_VIDEO_BREAK;
    } else if(!strcasecmp(name, "CF_VIDEO_ECHO")) {
        flag = CF_VIDEO_ECHO;
    } else if(!strcasecmp(name, "CF_VIDEO_BLANK")) {
        flag = CF_VIDEO_BLANK;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported flag: %s\n", name);
    }

    if(flag != CF_FLAG_MAX) {
        ret = switch_channel_test_flag(switch_core_session_get_channel(jss->session), flag) ? JS_TRUE: JS_FALSE;
    }

    JS_FreeCString(ctx, name);
    return ret;
}

static JSValue js_session_get_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    const char *terminators = NULL;
    char buf[513] = { 0 };
    uint32_t digits = 0, timeout = 5000, digit_timeout = 0, abs_timeout = 0;
    JSValue result;

    SESSION_SANITY_CHECK();

    if(argc > 0) {
        char term;
        JS_ToUint32(ctx, &digits, argv[0]);

        if(digits > sizeof(buf) - 1) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Exceeded max digits of %" SWITCH_SIZE_T_FMT "\n", sizeof(buf) - 1);
            return JS_UNDEFINED;
        }
        if(argc > 1) {
            terminators = JS_ToCString(ctx, argv[1]);
        }
        if(argc > 2) {
            JS_ToUint32(ctx, &timeout, argv[2]);
        }
        if(argc > 3) {
            JS_ToUint32(ctx, &digit_timeout, argv[3]);
        }
        if(argc > 4) {
            JS_ToUint32(ctx, &abs_timeout, argv[4]);
        }

        switch_ivr_collect_digits_count(jss->session, buf, sizeof(buf), digits, terminators, &term, timeout, digit_timeout, abs_timeout);
        result = JS_NewString(ctx, (char *) buf);

        JS_FreeCString(ctx, terminators);
        return result;
    }

    return JS_UNDEFINED;
}

static JSValue js_session_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    switch_channel_answer(switch_core_session_get_channel(jss->session));

    return JS_TRUE;
}

static JSValue js_session_pre_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();

    switch_channel_pre_answer(switch_core_session_get_channel(jss->session));

    return JS_TRUE;
}

static JSValue js_session_generate_xml_cdr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_xml_t cdr = NULL;
    JSValue result = JS_UNDEFINED;

    if(switch_ivr_generate_xml_cdr(jss->session, &cdr) == SWITCH_STATUS_SUCCESS) {
        char *xml_text;
        if((xml_text = switch_xml_toxml(cdr, false))) {
            result = JS_NewString(ctx, xml_text);
        }
        switch_safe_free(xml_text);
        switch_xml_free(cdr);
    }

    return result;
}

static JSValue js_session_get_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_event_t *event = NULL;

    SESSION_SANITY_CHECK();

    if(switch_core_session_dequeue_event(jss->session, &event, false) == SWITCH_STATUS_SUCCESS) {
        return js_event_object_create(ctx, event);
    }

    return JS_UNDEFINED;
}

// (jsEvent)
static JSValue js_session_put_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    if(argc > 0) {
        if(JS_IsObject(argv[0])) {
            js_event_t *js_event = JS_GetOpaque2(ctx, argv[0], js_event_get_classid(ctx));
            if(!js_event && !js_event->event) {
                return JS_FALSE;
            }
            if(switch_core_session_receive_event(jss->session, &js_event->event) != SWITCH_STATUS_SUCCESS) {
                return JS_FALSE;
            }
        }
    }

    return JS_FALSE;
}

static JSValue js_session_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;
    const char *cause_name = NULL;
    switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(switch_channel_up(channel)) {
        if(argc > 1) {
            if(JS_IsNumber(argv[0])) {
                int32_t i = 0;
                JS_ToUint32(ctx, &i, argv[0]);
                cause = i;
            } else {
                cause_name = JS_ToCString(ctx, argv[0]);
                cause = switch_channel_str2cause(cause_name);
                JS_FreeCString(ctx, cause_name);
            }
        }
        switch_channel_hangup(channel, cause);
        switch_core_session_kill_channel(jss->session, SWITCH_SIG_KILL);

        return JS_TRUE;
    }

    return JS_FALSE;
}

static JSValue js_session_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;
    JSValue result = JS_FALSE;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(argc > 0) {
        switch_application_interface_t *application_interface;
        const char *app_name = JS_ToCString(ctx, argv[0]);
        const char *app_arg = NULL;

        if(argc > 1) {
            app_arg = JS_ToCString(ctx, argv[1]);
        }

        if((application_interface = switch_loadable_module_get_application_interface(app_name))) {
            if(application_interface->application_function) {
                switch_core_session_exec(jss->session, application_interface, app_arg);
                UNPROTECT_INTERFACE(application_interface);
                result = JS_TRUE;
            }
        }
        JS_FreeCString(ctx, app_name);
        JS_FreeCString(ctx, app_arg);
    }

    return result;
}

// sleep(msec, [cbHook])
static JSValue js_session_sleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    int msec = 0;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc) {
        JS_ToUint32(ctx, &msec, argv[0]);
    }
    if(msec <= 0) {
        return JS_FALSE;
    }

    if(argc > 1 && JS_IsFunction(ctx, argv[1]) ) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
        cb_state.function = argv[1];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);
    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_ivr_sleep(jss->session, msec, false, &args);

    return JS_TRUE;
}

static JSValue js_session_gen_tones(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
        switch_input_args_t args = { 0 };
    const char *tone_script = NULL;
    char *buf = NULL, *p = NULL;
    int loops = 0;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();

    if(!argc) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: toneScript");
    }

    tone_script = JS_ToCString(ctx, argv[0]);
    buf = switch_core_session_strdup(jss->session, tone_script);
    JS_FreeCString(ctx, tone_script);

    if((p = strchr(buf, '|'))) {
        *p++ = '\0';
        loops = atoi(p);
        if(loops < 0) { loops = -1; }
    }

    if(argc > 1 && JS_IsFunction(ctx, argv[1]) ) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
        cb_state.function = argv[1];

        args.input_callback = xxx_input_callback;
        args.buf = &cb_state;
        args.buflen = sizeof(cb_state);

    }

    switch_channel_flush_dtmf(switch_core_session_get_channel(jss->session));
    switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");
    switch_ivr_gentones(jss->session, tone_script, loops, &args);

    return JS_TRUE;
}

static JSValue js_session_get_read_codec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    return js_codec_from_session_rcodec(ctx, jss->session);
}

static JSValue js_session_get_write_codec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));

    SESSION_SANITY_CHECK();

    return js_codec_from_session_wcodec(ctx, jss->session);
}

static JSValue js_session_frame_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_status_t status;
    switch_frame_t *read_frame = NULL;
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    SESSION_SANITY_CHECK();

    if(!argc)  {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &buf_size, argv[0]);
    if(!buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: buffer");
    }

    status = switch_core_session_read_frame(jss->session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
    if(SWITCH_READ_ACCEPTABLE(status) && read_frame->samples > 0 && !switch_test_flag(read_frame, SFF_CNG)) {
        len = (read_frame->datalen > buf_size ? buf_size : read_frame->datalen);
        memcpy(buf, read_frame->data, len);
    }

    return JS_NewInt64(ctx, len);
}

static JSValue js_session_frame_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_get_classid(ctx));
    switch_codec_t *wcodec = NULL;
    switch_frame_t write_frame = { 0 };
    switch_status_t status;
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    SESSION_SANITY_CHECK();

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &buf_size, argv[0]);
    if(!buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: buffer");
    }

    JS_ToInt64(ctx, &len, argv[1]);
    if(len <= 0) {
        return JS_NewInt64(ctx, 0);
    }
    if(len > buf_size) {
        return JS_ThrowRangeError(ctx, "len > buffer.size");
    }

    if(argc > 2) {
        js_codec_t *js_codec = JS_GetOpaque(argv[2], js_codec_get_classid(ctx));
        if(js_codec) {
            wcodec = js_codec->codec;
        }
    }
    if(!wcodec) {
        wcodec = switch_core_session_get_write_codec(jss->session);
        if(!wcodec) {
            return JS_ThrowRangeError(ctx, "No suitable codec");
        }
    }

    if(!jss->frame_buffer) {
        jss->frame_buffer_size = SWITCH_RECOMMENDED_BUFFER_SIZE;
        jss->frame_buffer = switch_core_session_alloc(jss->session, jss->frame_buffer_size);
    }
    if(jss->frame_buffer_size < len) {
        jss->frame_buffer_size = len;
        jss->frame_buffer = switch_core_session_alloc(jss->session, jss->frame_buffer_size);
    }

    memcpy(jss->frame_buffer, buf, len);

    write_frame.codec = wcodec;
    write_frame.buflen = buf_size;
    write_frame.samples = len;
    write_frame.datalen = len;
    write_frame.data = jss->frame_buffer;

    switch_core_session_write_frame(jss->session, &write_frame, SWITCH_IO_FLAG_NONE, 0);

    return JS_NewInt64(ctx, len);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// handlers
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static switch_status_t sys_session_hangup_hook(switch_core_session_t *session) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_state_t state = switch_channel_get_state(channel);
    js_session_t *jss = NULL;
    JSContext *ctx = NULL;
    JSValue args[1] = { 0 };
    JSValue ret_val;

    if(state == CS_HANGUP || state == CS_ROUTING) {
        if((jss = switch_channel_get_private(channel, "jss"))) {
            ctx = jss->ctx;
            if(jss->fl_hup_hook && JS_IsFunction(ctx, jss->on_hangup)) {
                args[0] = JS_NewInt32(ctx, state);
                ret_val = JS_Call(ctx, jss->on_hangup, JS_UNDEFINED, 1, (JSValueConst *) args);
                if(JS_IsException(ret_val)) {
                    js_ctx_dump_error(NULL, ctx);
                    JS_ResetUncatchableError(ctx);
                }
                JS_FreeValue(ctx, args[0]);
            }
        }
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t xxx_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    input_callback_state_t *cb_state = buf;
    js_session_t *jss = cb_state->jss;
    JSContext *ctx = (jss ? jss->ctx : NULL);
    JSValue args[4] = { 0 };
    JSValue jss_obj;
    JSValue ret_val;

    if(cb_state->jss_a && cb_state->jss_a->session && cb_state->jss_a->session == session) {
        jss_obj = cb_state->jss_a_obj;
    } else if(cb_state->jss_b && cb_state->jss_b->session && cb_state->jss_b->session == session) {
        jss_obj = cb_state->jss_b_obj;
    } else {
        jss_obj = cb_state->jss_obj;
    }

    if(itype == SWITCH_INPUT_TYPE_DTMF) {
        switch_dtmf_t *dtmf = (switch_dtmf_t *) input;

        if(dtmf->digit) {
            char digit[2] = { dtmf->digit, 0x0 };

            args[0] = jss_obj;
            args[1] = JS_NewString(ctx, "dtmf");
            args[2] = JS_NewString(ctx, (char *)digit);
            args[3] = cb_state->arg;

            ret_val = JS_Call(ctx, cb_state->function, JS_UNDEFINED, 4, (JSValueConst *) args);

            if(JS_IsException(ret_val)) {
                js_ctx_dump_error(NULL, ctx);
                JS_ResetUncatchableError(ctx);
            } else if(JS_IsBool(ret_val)) {
                status = (JS_ToBool(ctx, ret_val) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE);
            }

            JS_FreeValue(ctx, args[1]);
            JS_FreeValue(ctx, args[2]);
            JS_FreeValue(ctx, ret_val);
        }
    } else {
        //
        // todo
        //
    }

    return status;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_session_class = {
    CLASS_NAME,
    .finalizer = js_session_finalizer,
};

static const JSCFunctionListEntry js_session_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", js_session_property_get, js_session_property_set, PROP_NAME),
    JS_CGETSET_MAGIC_DEF("uuid", js_session_property_get, js_session_property_set, PROP_UUID),
    JS_CGETSET_MAGIC_DEF("state", js_session_property_get, js_session_property_set, PROP_STATE),
    JS_CGETSET_MAGIC_DEF("cause", js_session_property_get, js_session_property_set, PROP_CAUSE),
    JS_CGETSET_MAGIC_DEF("causecode", js_session_property_get, js_session_property_set, PROP_CAUSECODE),
    JS_CGETSET_MAGIC_DEF("dialplan", js_session_property_get, js_session_property_set, PROP_PROFILE_DIALPLAN),
    JS_CGETSET_MAGIC_DEF("destination", js_session_property_get, js_session_property_set, PROP_PROFILE_DESTINATION),
    JS_CGETSET_MAGIC_DEF("callerIdName", js_session_property_get, js_session_property_set, PROP_CALLER_ID_NAME),
    JS_CGETSET_MAGIC_DEF("callerIdNumber", js_session_property_get, js_session_property_set, PROP_CALLER_ID_NUMBER),
    JS_CGETSET_MAGIC_DEF("readCodecName", js_session_property_get, js_session_property_set, PROP_RCODEC_NAME),
    JS_CGETSET_MAGIC_DEF("writeCodecName", js_session_property_get, js_session_property_set, PROP_WCODEC_NAME),
    JS_CGETSET_MAGIC_DEF("samplerate", js_session_property_get, js_session_property_set, PROP_SAMPLERATE),
    JS_CGETSET_MAGIC_DEF("channels", js_session_property_get, js_session_property_set, PROP_CHANNELS),
    JS_CGETSET_MAGIC_DEF("ptime", js_session_property_get, js_session_property_set, PROP_PTIME),
    JS_CGETSET_MAGIC_DEF("isReady", js_session_property_get, js_session_property_set, PROP_IS_READY),
    JS_CGETSET_MAGIC_DEF("isAnswered", js_session_property_get, js_session_property_set, PROP_IS_ANSWERED),
    JS_CGETSET_MAGIC_DEF("isMediaReady", js_session_property_get, js_session_property_set, PROP_IS_MEDIA_READY),
    JS_CGETSET_MAGIC_DEF("encodedFrameSize", js_session_property_get, js_session_property_set, PROP_ENCODED_FRAME_SIZE),
    JS_CGETSET_MAGIC_DEF("decodedFrameSize", js_session_property_get, js_session_property_set, PROP_DECODED_FRAME_SIZE),
    JS_CGETSET_MAGIC_DEF("ttsEngine", js_session_property_get, js_session_property_set, PROP_TTS_ENGINE),
    JS_CGETSET_MAGIC_DEF("asrEngine", js_session_property_get, js_session_property_set, PROP_ASR_ENGINE),
    JS_CGETSET_MAGIC_DEF("language", js_session_property_get, js_session_property_set, PROP_LANGUAGE),
    JS_CGETSET_MAGIC_DEF("bgStreams", js_session_property_get, js_session_property_set, PROP_BG_STREAMS),
    JS_CGETSET_MAGIC_DEF("autoHangup", js_session_property_get, js_session_property_set, PROP_AUTO_HANGUP),
    //
    JS_CFUNC_DEF("setHangupHook", 1, js_session_set_hangup_hook),
    JS_CFUNC_DEF("speak", 1, js_session_speak),
    JS_CFUNC_DEF("speakEx", 1, js_session_speak_ex),
    JS_CFUNC_DEF("playback", 1, js_session_playback),
    //JS_CFUNC_DEF("playbackCtl", 1, js_session_playback_ctl),
    JS_CFUNC_DEF("playbackStop", 1, js_session_playback_stop),
    JS_CFUNC_DEF("bgPlaybackStart", 1, js_session_bg_playback_start),
    JS_CFUNC_DEF("bgPlaybackStop", 1, js_session_bg_playback_stop),
    JS_CFUNC_DEF("bgPlaybackCtl", 1, js_session_bg_playback_ctl),
    JS_CFUNC_DEF("playAndDetectSpeech", 1, js_session_play_and_detect_speech),
    JS_CFUNC_DEF("sayAndDetectSpeech", 1, js_session_say_and_detect_speech),
    JS_CFUNC_DEF("detectSpeech", 1, js_session_detect_speech),
    JS_CFUNC_DEF("detectSpeechEx", 1, js_session_detect_speech_ex),
    JS_CFUNC_DEF("recordFile", 1, js_session_record_file),
    JS_CFUNC_DEF("collectInput", 1, js_session_collect_input),
    JS_CFUNC_DEF("flushEvents", 1, js_session_flush_events),
    JS_CFUNC_DEF("flushDigits", 1, js_session_flush_digits),
    JS_CFUNC_DEF("setVariable", 2, js_session_set_var),
    JS_CFUNC_DEF("getVariable", 1, js_session_get_var),
    JS_CFUNC_DEF("setChanFlag", 2, js_session_set_chan_flag),
    JS_CFUNC_DEF("getChanFlag", 1, js_session_get_chan_flag),
    JS_CFUNC_DEF("getDigits", 4, js_session_get_digits),
    JS_CFUNC_DEF("answer", 0, js_session_answer),
    JS_CFUNC_DEF("preAnswer", 0, js_session_pre_answer),
    JS_CFUNC_DEF("getEvent", 0, js_session_get_event),
    JS_CFUNC_DEF("putEvent", 0, js_session_put_event),
    JS_CFUNC_DEF("hangup", 0, js_session_hangup),
    JS_CFUNC_DEF("execute", 0, js_session_execute),
    JS_CFUNC_DEF("sleep", 1, js_session_sleep),
    JS_CFUNC_DEF("genTones", 1, js_session_gen_tones),
    JS_CFUNC_DEF("getReadCodec", 0, js_session_get_read_codec),
    JS_CFUNC_DEF("getWriteCodec", 0, js_session_get_write_codec),
    JS_CFUNC_DEF("frameRead", 1, js_session_frame_read),
    JS_CFUNC_DEF("frameWrite", 1, js_session_frame_write),
    //
    JS_CFUNC_DEF("generateXmlCdr", 0, js_session_generate_xml_cdr),
    JS_CFUNC_DEF("playAndGetDigits", 1, js_session_play_and_get_digits),
    JS_CFUNC_DEF("sayPhrase", 1, js_session_say_phrase)
};

static void js_session_finalizer(JSRuntime *rt, JSValue val) {
    js_session_t *jss = JS_GetOpaque(val, js_session_get_classid2(rt));
    uint8_t fl_wloop = false;

    if(!jss) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "(jss == NULL)\n");
        return;
    }

    jss->fl_ready = false;

    if(jss->bg_streams) {
        js_session_bgs_stream_stop(jss);
    }

    if(jss->mutex) {
        switch_mutex_lock(jss->mutex);
        fl_wloop = (jss->wlock > 0);
        switch_mutex_unlock(jss->mutex);

        if(fl_wloop) {
#ifdef MOD_QUICKJS_DEBUG
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for unlock ('%d' locks) ...\n", jss->wlock);
#endif

            while(fl_wloop) {
                switch_mutex_lock(jss->mutex);
                fl_wloop = (jss->wlock > 0);
                switch_mutex_unlock(jss->mutex);
                switch_yield(100000);
            }
        }
    }

    if(jss->session) {
        switch_channel_t *channel = switch_core_session_get_channel(jss->session);

        switch_channel_set_private(channel, "jss", NULL);
        switch_core_event_hook_remove_state_change(jss->session, sys_session_hangup_hook);

        if(jss->fl_hup_auto) {
            switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        if(jss->fl_no_unlock == false) {
            switch_core_session_rwunlock(jss->session);
        }
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-finalizer: jss=%p, session=%p\n", jss, jss->session);
#endif

    js_free_rt(rt, jss);
}

/* new (uuid | originateString, [oldSession, originateTimeout]) */
static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto, error;
    js_session_t *jss = NULL;
    js_session_t *jss_old = NULL;
    switch_call_cause_t h_cause;
    switch_codec_implementation_t read_impl = { 0 };
    uint8_t has_error = 0;
    uint32_t tosec = 0;
    const char *data;

    if(!argc) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    jss = js_mallocz(ctx, sizeof(js_session_t));
    if(!jss) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        return JS_EXCEPTION;
    }

    data = JS_ToCString(ctx, argv[0]);
    if(!strchr(data, '/')) {
        jss->session = switch_core_session_locate(data);
        jss->fl_hup_auto = true;

        if(!jss->session) {
            error = JS_ThrowTypeError(ctx, "Unable to create a new session (not found)");
            has_error = true; goto fail;
        }
    } else {
        if(argc > 1 && JS_IsObject(argv[1])) {
            jss_old = JS_GetOpaque2(ctx, argv[1], js_session_get_classid(ctx));
        }
        if(argc > 2)  {
            JS_ToInt32(ctx, &tosec, argv[2]);
        }
        if(tosec == 0) {
            tosec = 60;
        }

        if(switch_ivr_originate((jss_old ? jss_old->session : NULL), &jss->session, &h_cause, data, tosec, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) == SWITCH_STATUS_SUCCESS) {
            jss->fl_hup_auto = true;
            switch_channel_set_state(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE);
            switch_channel_wait_for_state_timeout(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE, 5000);
        } else {
            //error = JS_ThrowTypeError(ctx, "Originate failed: %s", switch_channel_cause2str(h_cause));
            //has_error = true; goto fail;

            jss->originate_fail_code = h_cause;
            jss->fl_originate_fail_result = true;

            proto = JS_GetPropertyStr(ctx, new_target, "prototype");
            if(JS_IsException(proto)) { goto fail; }

            obj = JS_NewObjectProtoClass(ctx, proto, js_session_get_classid(ctx));
            JS_FreeValue(ctx, proto);
            if(JS_IsException(obj)) { goto fail; }

            goto out;
        }
    }

    if(!jss->session) {
        error = JS_ThrowTypeError(ctx, "Unable to create a new session");
        has_error = true;
        goto fail;
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_session_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }


    switch_mutex_init(&jss->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(jss->session));
    switch_core_session_get_read_impl(jss->session, &read_impl);

    jss->ctx = ctx;
    jss->session_id = switch_core_session_get_uuid(jss->session);
    jss->ptime = (read_impl.microseconds_per_packet / 1000);
    jss->channels = read_impl.number_of_channels;
    jss->samplerate = read_impl.samples_per_second;
    jss->encoded_frame_size = read_impl.encoded_bytes_per_packet;
    jss->decoded_frame_size = read_impl.decoded_bytes_per_packet;

    jss->fl_ready = true;

out:
    JS_SetOpaque(obj, jss);
    JS_FreeCString(ctx, data);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-constructor: jss=%p, session=%p, originate_fail_code=%i\n", jss, jss->session, jss->originate_fail_code);
#endif

    return obj;

fail:
    JS_FreeCString(ctx, data);

    if(jss) {
        js_free(ctx, jss);
    }
    JS_FreeValue(ctx, obj);

    return (has_error ? error : JS_EXCEPTION);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_session_get_classid2(JSRuntime *rt) {
    script_t *script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);
    return script->class_id_session;
}
JSClassID js_session_get_classid(JSContext *ctx) {
    return  js_session_get_classid2(JS_GetRuntime(ctx));
}

switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id) {
    JSValue obj_proto, obj_class;
    script_t *script = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    switch_assert(script);

    if(JS_IsRegisteredClass(JS_GetRuntime(ctx), class_id)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Class with id (%d) already registered!\n", class_id);
        return SWITCH_STATUS_FALSE;
    }

    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, &js_session_class);
    script->class_id_session = class_id;

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Class registered [%s / %d]\n", CLASS_NAME, class_id);
#endif

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_session_proto_funcs, ARRAY_SIZE(js_session_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_session_contructor, CLASS_NAME, 2, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session) {
    switch_codec_implementation_t read_impl = { 0 };
    js_session_t *jss = NULL;
    JSValue obj, proto;

    jss = js_mallocz(ctx, sizeof(js_session_t));
    if(!jss) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_session_proto_funcs, ARRAY_SIZE(js_session_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_session_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { return obj; }

    //
    switch_mutex_init(&jss->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));
    switch_core_session_get_read_impl(session, &read_impl);

    jss->ctx = ctx;
    jss->session = session;
    jss->session_id = switch_core_session_get_uuid(session);
    jss->ptime = (read_impl.microseconds_per_packet / 1000);
    jss->channels = read_impl.number_of_channels;
    jss->samplerate = read_impl.samples_per_second;
    jss->encoded_frame_size = read_impl.encoded_bytes_per_packet;
    jss->decoded_frame_size = read_impl.decoded_bytes_per_packet;

    jss->fl_ready = true;
    jss->fl_hup_auto = true;
    jss->fl_no_unlock = true;

    JS_SetOpaque(obj, jss);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-obj-created: jss=%p, session=%p\n", jss, jss->session);
#endif

    return obj;
}

JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss_a = NULL;
    js_session_t *jss_b = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    JSClassID class_id = 0;
    int len = 0;
    void *bp = NULL;

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    class_id = js_session_get_classid(ctx);

    jss_a = JS_GetOpaque(argv[0], class_id);
    if(!(jss_a && jss_a->session)) {
        return JS_ThrowTypeError(ctx, "Session A is not ready");
    }

    jss_b = JS_GetOpaque(argv[1], class_id);
    if(!(jss_b && jss_b->session)) {
        return JS_ThrowTypeError(ctx, "Session B is not ready");
    }

    if(argc > 2 && JS_IsFunction(ctx, argv[2])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss = jss_a;
        cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
        cb_state.function = argv[1];

        cb_state.jss_a_obj = argv[0];
        cb_state.jss_b_obj = argv[1];
        cb_state.jss_a = jss_a;
        cb_state.jss_b = jss_b;

        dtmf_func = xxx_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    switch_ivr_multi_threaded_bridge(jss_a->session, jss_b->session, dtmf_func, bp, bp);
    return JS_FALSE;
}

