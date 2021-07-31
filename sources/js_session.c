/**
 * Session object
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME                  "Session"
#define PROP_NAME                   0
#define PROP_UUID                   1
#define PROP_STATE                  2
#define PROP_CAUSE                  3
#define PROP_CAUSECODE              4
#define PROP_CALLER_ID_NAME         5
#define PROP_CALLER_ID_NUMBER       6
#define PROP_PROFILE_DIALPLAN       7
#define PROP_PROFILE_DESTINATION    8
#define PROP_RCODEC_NAME            9
#define PROP_WCODEC_NAME            10
#define PROP_SAMPLERATE             11
#define PROP_CHANNELS               12
#define PROP_PTIME                  13
#define PROP_IS_READY               14
#define PROP_IS_ANSWERED            15
#define PROP_IS_MEDIA_READY         16

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

static JSClassID js_session_class_id;

static void js_session_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static switch_status_t js_collect_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
static switch_status_t js_playback_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
static switch_status_t js_record_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
static switch_status_t sys_session_hangup_hook(switch_core_session_t *session);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_session_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    switch_caller_profile_t *caller_profile = NULL;
    switch_codec_implementation_t read_impl = { 0 };

    if(magic == PROP_IS_READY) {
        uint8_t x = (jss && jss->session && switch_channel_ready(switch_core_session_get_channel(jss->session)));
        return(x ? JS_TRUE : JS_FALSE);
    }

    SESSION_SANITY_CHECK();

    channel = switch_core_session_get_channel(jss->session);
    caller_profile = switch_channel_get_caller_profile(channel);

    switch(magic) {
        case PROP_NAME:
            return JS_NewString(ctx, switch_channel_get_name(channel));

        case PROP_UUID:
            return JS_NewString(ctx, switch_channel_get_uuid(channel));

        case PROP_STATE:
            return JS_NewString(ctx, switch_channel_state_name(switch_channel_get_state(channel)) );

        case PROP_CAUSE:
            return JS_NewString(ctx, switch_channel_cause2str(switch_channel_get_cause(channel)) );

        case PROP_CAUSECODE:
            return JS_NewInt32(ctx, switch_channel_get_cause(channel));

        case PROP_CALLER_ID_NAME:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->caller_id_name);

        case PROP_CALLER_ID_NUMBER:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->caller_id_number);

        case PROP_PROFILE_DIALPLAN:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->dialplan);

        case PROP_PROFILE_DESTINATION:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->destination_number);

        case PROP_SAMPLERATE:
            switch_core_session_get_read_impl(jss->session, &read_impl);
            return JS_NewInt32(ctx, read_impl.samples_per_second);

        case PROP_CHANNELS:
            switch_core_session_get_read_impl(jss->session, &read_impl);
            return JS_NewInt32(ctx, read_impl.number_of_channels);

        case PROP_PTIME:
            switch_core_session_get_read_impl(jss->session, &read_impl);
            return JS_NewInt32(ctx, read_impl.microseconds_per_packet / 1000);

        case PROP_RCODEC_NAME: {
            const char *name = switch_channel_get_variable(channel, "read_codec");
            if(!zstr(name)) { JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }

        case PROP_WCODEC_NAME: {
            const char *name = switch_channel_get_variable(channel, "write_codec");
            if(!zstr(name)) { JS_NewString(ctx, name); }
            return JS_UNDEFINED;
        }

        case PROP_IS_ANSWERED: {
            return (switch_channel_test_flag(switch_core_session_get_channel(jss->session), CF_ANSWERED) ? JS_TRUE : JS_FALSE);
        }

        case PROP_IS_MEDIA_READY: {
            return (switch_channel_media_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_session_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_FALSE;
}

static JSValue js_session_set_hangup_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid argument: callback function");
    }

    if(JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        jss->fl_hup_hook = SWITCH_FALSE;

    } else if(JS_IsFunction(ctx, argv[0])) {
        jss->on_hangup = argv[0];
        jss->fl_hup_hook = SWITCH_TRUE;

        switch_channel_set_private(channel, "jss", jss);
        switch_core_event_hook_add_state_change(jss->session, sys_session_hangup_hook);
    }

    return JS_TRUE;
}

static JSValue js_session_set_auto_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();

    if(argc > 0) {
        jss->fl_hup_auto = JS_ToBool(ctx, argv[0]);
    }
    return JS_TRUE;
}

static JSValue  js_session_speak(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    const char *js_tts_name = NULL, *js_tts_voice = NULL;
    const char *ch_tts_name = NULL, *ch_tts_voice = NULL;
    const char *tts_text = NULL;
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    void *bp = NULL;
    int len = 0;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc < 1) {
        return JS_FALSE;
    }

    js_tts_name = JS_ToCString(ctx, argv[0]);
    if(zstr(js_tts_name)) {
        ch_tts_name = switch_channel_get_variable(channel, "tts_engine");
        if(zstr(ch_tts_name)) {
            return JS_ThrowTypeError(ctx, "Invalid argument: TTS Name");
        }
    }

    js_tts_voice = JS_ToCString(ctx, argv[1]);
    if(zstr(js_tts_voice)) {
         ch_tts_voice = switch_channel_get_variable(channel, "tts_voice");
            if(zstr(ch_tts_name)) {
                JS_FreeCString(ctx, js_tts_voice);
                return JS_ThrowTypeError(ctx, "Invalid argument: TTS Voice");
            }
    }

    tts_text = JS_ToCString(ctx, argv[2]);
    if(zstr(tts_text)) {
        JS_FreeCString(ctx, js_tts_name);
        JS_FreeCString(ctx, js_tts_voice);
        return JS_ThrowTypeError(ctx, "Invalid argument: TTS Text");
    }

    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_speak_text(jss->session, (js_tts_name ? js_tts_name : ch_tts_name), (js_tts_voice ? js_tts_voice : ch_tts_voice), tts_text, &args);

    JS_FreeCString(ctx, js_tts_name);
    JS_FreeCString(ctx, js_tts_voice);
    JS_FreeCString(ctx, tts_text);

    return JS_TRUE;
}

static JSValue js_session_say_phrase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    const char *phrase_name = NULL, *phrase_data = NULL, *phrase_lang = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    void *bp = NULL;
    int len = 0;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc > 0) {
        phrase_name = JS_ToCString(ctx, argv[0]);
        if(zstr(phrase_name)) { return JS_FALSE; }
    }
    if(argc > 1) {
        phrase_data = JS_ToCString(ctx, argv[1]);
    }
    if(argc > 2) {
        phrase_lang = JS_ToCString(ctx, argv[2]);
    }
    if(argc > 3 && JS_IsFunction(ctx, argv[3])) {
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 4 ? argv[4] : JS_UNDEFINED);
        cb_state.function = argv[3];

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_phrase_macro(jss->session, phrase_name, phrase_data, phrase_lang, &args);

    JS_FreeCString(ctx, phrase_name);
    JS_FreeCString(ctx, phrase_data);
    JS_FreeCString(ctx, phrase_lang);

    return JS_TRUE;
}

static JSValue js_session_stream_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    char *file_obj_fname = NULL;
    const char *file_name = NULL;
    const char *prebuf = NULL;
    char posbuf[64] = "";
    input_callback_state_t cb_state = { 0 };
    switch_file_handle_t fh = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    uint32_t len = 0;
    void *bp = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc > 0) {
        js_file_t *js_file = JS_GetOpaque(argv[0], js_file_class_get_id());
        if(js_file) {
            file_obj_fname = strdup(js_file->path);
        } else {
            file_name = JS_ToCString(ctx, argv[0]);
            if(zstr(file_name)) {
                return JS_FALSE;
            }
        }
    }
    if(argc > 1) {
        if(JS_IsFunction(ctx, argv[1])) {
            memset(&cb_state, 0, sizeof(cb_state));
            cb_state.jss = jss;
            cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
            cb_state.function = argv[1];
            cb_state.fh_obj = js_file_handle_object_create(ctx, &fh, jss->session);

            dtmf_func = js_playback_input_callback;
            bp = &cb_state;
            len = sizeof(cb_state);
        }
    }
    if(argc > 3) {
        uint32_t samps;
        JS_ToUint32(ctx, &samps, argv[3]);
        fh.samples = samps;
    }

    if((prebuf = switch_channel_get_variable(channel, "stream_prebuffer"))) {
        int maybe = atoi(prebuf);
        if(maybe > 0) { fh.prebuf = maybe; }
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_play_file(jss->session, &fh, (file_name ? file_name : file_obj_fname), &args);

    JS_FreeValue(ctx, cb_state.fh_obj);
    JS_FreeCString(ctx, file_name);
    switch_safe_free(file_obj_fname);

    switch_snprintf(posbuf, sizeof(posbuf), "%u", fh.offset_pos);
    switch_channel_set_variable(channel, "last_file_position", posbuf);

    return JS_TRUE;
}

static JSValue js_session_record_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    char *file_obj_fname = NULL;
    const char *file_name = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_file_handle_t fh = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    uint32_t limit = 0, len = 0;
    void *bp = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc > 0) {
        js_file_t *js_file = JS_GetOpaque(argv[0], js_file_class_get_id());
        if(js_file) {
            file_obj_fname = strdup(js_file->path);
        } else {
            file_name = JS_ToCString(ctx, argv[0]);
            if(zstr(file_name)) {
                return JS_FALSE;
            }
        }
    }
    if(argc > 1) {
        if(JS_IsFunction(ctx, argv[1])) {
            memset(&cb_state, 0, sizeof(cb_state));
            cb_state.jss = jss;
            cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
            cb_state.function = argv[1];
            cb_state.fh_obj = js_file_handle_object_create(ctx, &fh, jss->session);

            dtmf_func = js_record_input_callback;
            bp = &cb_state;
            len = sizeof(cb_state);
        }
        if(argc > 3) {
            JS_ToUint32(ctx, &limit, argv[3]);
        }
        if(argc > 4) {
            uint32_t thresh;
            JS_ToUint32(ctx, &thresh, argv[4]);
            fh.thresh = thresh;
        }
        if(argc > 5) {
            uint32_t silence_hits;
            JS_ToUint32(ctx, &silence_hits, argv[5]);
            fh.silence_hits = silence_hits;
        }
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_record_file(jss->session, &fh, (file_name ? file_name : file_obj_fname), &args, limit);

    JS_FreeValue(ctx, cb_state.fh_obj);
    JS_FreeCString(ctx, file_name);
    switch_safe_free(file_obj_fname);

    return JS_TRUE;
}

static JSValue js_session_collect_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    switch_input_args_t args = { 0 };
    uint32_t abs_timeout = 0, digit_timeout = 0;
    void *bp = NULL;
    int len = 0;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc > 0) {
        if(!JS_IsFunction(ctx, argv[0])) {
            return JS_ThrowTypeError(ctx, "Invalid argument: callback function");
        }
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.jss_obj = this_val;
        cb_state.jss = jss;
        cb_state.arg = (argc > 1 ? argv[1] : JS_UNDEFINED);
        cb_state.function = argv[0];

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }
    if(argc == 3) {
        JS_ToUint32(ctx, &abs_timeout, argv[2]);
    } else if(argc > 3) {
        JS_ToUint32(ctx, &digit_timeout, argv[2]);
        JS_ToUint32(ctx, &abs_timeout, argv[3]);
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_collect_digits_callback(jss->session, &args, digit_timeout, abs_timeout);

    return JS_TRUE;
}

static JSValue js_session_flush_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_event_t *event;

    SESSION_SANITY_CHECK();
    while(switch_core_session_dequeue_event(jss->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
        switch_event_destroy(&event);
    }

    return JS_TRUE;
}

static JSValue js_session_flush_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    switch_channel_flush_dtmf(channel);

    return JS_TRUE;
}

static JSValue js_session_set_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();

    channel = switch_core_session_get_channel(jss->session);
    if(argc >= 2) {
        const char *var, *val;

        var = JS_ToCString(ctx, argv[0]);
        val = JS_ToCString(ctx, argv[1]);

        switch_channel_set_variable_var_check(channel, var, val, SWITCH_FALSE);

        JS_FreeCString(ctx, var);
        JS_FreeCString(ctx, val);

        return JS_TRUE;
    }

    return JS_FALSE;
}

static JSValue js_session_get_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(argc >= 1) {
        const char *var;
        const char *val;

        var = JS_ToCString(ctx, argv[0]);
        val = switch_channel_get_variable(channel, var);
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
    }

    return JS_UNDEFINED;
}

static JSValue js_session_get_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    const char *terminators = NULL;
    char buf[513] = { 0 };
    uint32_t digits = 0, timeout = 5000, digit_timeout = 0, abs_timeout = 0;
    JSValue result;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

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
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK_ANSWER();

    switch_channel_answer(channel);

    return JS_TRUE;
}

static JSValue js_session_pre_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK_ANSWER();

    switch_channel_pre_answer(channel);

    return JS_TRUE;
}

static JSValue js_session_generate_xml_cdr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_xml_t cdr = NULL;
    JSValue result = JS_UNDEFINED;

    if(switch_ivr_generate_xml_cdr(jss->session, &cdr) == SWITCH_STATUS_SUCCESS) {
        char *xml_text;
        if((xml_text = switch_xml_toxml(cdr, SWITCH_FALSE))) {
            result = JS_NewString(ctx, xml_text);
        }
        switch_safe_free(xml_text);
        switch_xml_free(cdr);
    }

    return result;
}

static JSValue js_session_is_answered(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_test_flag(switch_core_session_get_channel(jss->session), CF_ANSWERED) ? JS_TRUE : JS_FALSE);
}

static JSValue js_session_is_media_ready(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_media_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
}

static JSValue js_session_get_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_event_t *event = NULL;

    SESSION_SANITY_CHECK();

    if(switch_core_session_dequeue_event(jss->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
        return js_event_object_create(ctx, event);
    }

    return JS_UNDEFINED;
}

static JSValue js_session_send_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    SESSION_SANITY_CHECK();

    if(argc > 0) {
        if(JS_IsObject(argv[0])) {
            js_event_t *js_event = JS_GetOpaque2(ctx, argv[0], js_event_class_get_id());

            if(!js_event && !js_event->event) {
                return JS_FALSE;
            }
            if(switch_core_session_receive_event(jss->session, &js_event->event) != SWITCH_STATUS_SUCCESS) {
                return JS_FALSE;
            }
            //js_event->event = NULL
        }
    }

    return JS_FALSE;
}

static JSValue js_session_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
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
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
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

static JSValue js_session_sleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    switch_input_args_t args = { 0 };
    int len = 0, msec = 0;
    void *bp = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();
    CHANNEL_MEDIA_SANITY_CHECK();

    if(argc > 0) {
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

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_sleep(jss->session, msec, SWITCH_FALSE, &args);

    return JS_TRUE;
}

static JSValue js_session_gen_tones(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    switch_input_args_t args = { 0 };
    const char *tone_script = NULL;
    int len = 0, loops = 0;
    char *buf = NULL, *p = NULL;
    void *bp = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    tone_script = JS_ToCString(ctx, argv[0]);
    if(zstr(tone_script)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: toneScript");
    }

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

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_channel_set_variable(channel, SWITCH_PLAYBACK_TERMINATOR_USED, "");
    switch_ivr_gentones(jss->session, tone_script, loops, &args);

    return JS_TRUE;
}

static JSValue js_session_get_read_codec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    SESSION_SANITY_CHECK();

    return js_codec_from_session_rcodec(ctx, jss->session);
}

static JSValue js_session_get_write_codec(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    SESSION_SANITY_CHECK();

    return js_codec_from_session_wcodec(ctx, jss->session);
}

static JSValue js_session_frame_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_status_t status;
    switch_frame_t *read_frame = NULL;
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    SESSION_SANITY_CHECK();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
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
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_codec_t *wcodec = NULL;
    switch_frame_t write_frame = { 0 };
    switch_status_t status;
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    SESSION_SANITY_CHECK();

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
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
        return JS_ThrowRangeError(ctx, "Buffer overflow (len > array size)");
    }

    if(argc > 2) {
        js_codec_t *js_codec = JS_GetOpaque(argv[2], js_codec_class_get_id());
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

static JSValue js_session_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return js_session_hangup(ctx, this_val, argc, argv);
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
                    ctx_dump_error(NULL, NULL, ctx);
                    JS_ResetUncatchableError(ctx);
                }
                JS_FreeValue(ctx, args[0]);
            }
        }
    }

    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t js_record_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    input_callback_state_t *cb_state = buf;
    js_session_t *jss = cb_state->jss;
    JSContext *ctx = (jss ? jss->ctx : NULL);
    JSValue args[4] = { 0 };
    JSValue ret_val;

    if(itype == SWITCH_INPUT_TYPE_DTMF) {
        args[0] = cb_state->fh_obj;
        args[1] = js_dtmf_object_create(ctx, (switch_dtmf_t *) input);
        args[2] = JS_NewString(ctx, "dtmf");
        args[3] = cb_state->arg;

        ret_val = JS_Call(ctx, cb_state->function, JS_UNDEFINED, 4, (JSValueConst *) args);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(NULL, NULL, ctx);
            JS_ResetUncatchableError(ctx);
        } else if(JS_IsBool(ret_val)) {
            status = (JS_ToBool(ctx, ret_val) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE);
        }

        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
        JS_FreeValue(ctx, ret_val);
    }

    return status;
}

static switch_status_t js_playback_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    input_callback_state_t *cb_state = buf;
    js_session_t *jss = cb_state->jss;
    JSContext *ctx = (jss ? jss->ctx : NULL);
    JSValue args[4] = { 0 };
    JSValue ret_val;

    if(itype == SWITCH_INPUT_TYPE_DTMF) {
        args[0] = cb_state->fh_obj;
        args[1] = js_dtmf_object_create(ctx, (switch_dtmf_t *) input);
        args[2] = JS_NewString(ctx, "dtmf");
        args[3] = cb_state->arg;

        ret_val = JS_Call(ctx, cb_state->function, JS_UNDEFINED, 4, (JSValueConst *) args);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(NULL, NULL, ctx);
            JS_ResetUncatchableError(ctx);
        } else if(JS_IsBool(ret_val)) {
            status = (JS_ToBool(ctx, ret_val) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE);
        }

        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
        JS_FreeValue(ctx, ret_val);
    }

    return status;
}

static switch_status_t js_collect_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
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
        args[0] = jss_obj;
        args[1] = js_dtmf_object_create(ctx, (switch_dtmf_t *) input);
        args[2] = JS_NewString(ctx, "dtmf");
        args[3] = cb_state->arg;

        ret_val = JS_Call(ctx, cb_state->function, JS_UNDEFINED, 4, (JSValueConst *) args);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(NULL, NULL, ctx);
            JS_ResetUncatchableError(ctx);
        } else if(JS_IsBool(ret_val)) {
            status = (JS_ToBool(ctx, ret_val) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE);
        }

        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, args[2]);
        JS_FreeValue(ctx, ret_val);
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
    //
    JS_CFUNC_DEF("setHangupHook", 1, js_session_set_hangup_hook),
    JS_CFUNC_DEF("setAutoHangup", 1, js_session_set_auto_hangup),
    JS_CFUNC_DEF("speak", 1, js_session_speak),
    JS_CFUNC_DEF("sayPhrase", 1, js_session_say_phrase),
    JS_CFUNC_DEF("streamFile", 1, js_session_stream_file),
    JS_CFUNC_DEF("recordFile", 1, js_session_record_file),
    JS_CFUNC_DEF("collectInput", 1, js_session_collect_input),
    JS_CFUNC_DEF("flushEvents", 1, js_session_flush_events),
    JS_CFUNC_DEF("flushDigits", 1, js_session_flush_digits),
    JS_CFUNC_DEF("setVariable", 2, js_session_set_var),
    JS_CFUNC_DEF("getVariable", 1, js_session_get_var),
    JS_CFUNC_DEF("getDigits", 4, js_session_get_digits),
    JS_CFUNC_DEF("answer", 0, js_session_answer),
    JS_CFUNC_DEF("preAnswer", 0, js_session_pre_answer),
    JS_CFUNC_DEF("generateXmlCdr", 0, js_session_generate_xml_cdr),
    JS_CFUNC_DEF("getEvent", 0, js_session_get_event),
    JS_CFUNC_DEF("sendEvent", 0, js_session_send_event),
    JS_CFUNC_DEF("hangup", 0, js_session_hangup),
    JS_CFUNC_DEF("execute", 0, js_session_execute),
    JS_CFUNC_DEF("sleep", 1, js_session_sleep),
    JS_CFUNC_DEF("genTones", 1, js_session_gen_tones),
    JS_CFUNC_DEF("getReadCodec", 0, js_session_get_read_codec),
    JS_CFUNC_DEF("getWriteCodec", 0, js_session_get_write_codec),
    JS_CFUNC_DEF("frameRead", 1, js_session_frame_read),
    JS_CFUNC_DEF("frameWrite", 1, js_session_frame_write),
    // deprecated
    JS_CFUNC_DEF("answered", 0, js_session_is_answered),
    JS_CFUNC_DEF("mediaReady", 0, js_session_is_media_ready),
    JS_CFUNC_DEF("destroy", 0, js_session_destroy),
};

static void js_session_finalizer(JSRuntime *rt, JSValue val) {
    js_session_t *jss = JS_GetOpaque(val, js_session_class_id);

    if(!jss) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-finalizer: jss=%p, session=%p\n", jss, jss->session);

    if(jss->session) {
        switch_channel_t *channel = switch_core_session_get_channel(jss->session);

        switch_channel_set_private(channel, "jss", NULL);
        switch_core_event_hook_remove_state_change(jss->session, sys_session_hangup_hook);

        if(jss->fl_hup_auto) {
            switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        switch_core_session_rwunlock(jss->session);
    }
    js_free_rt(rt, jss);
}

static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto, error;
    js_session_t *jss = NULL;
    js_session_t *jss_old = NULL;
    switch_call_cause_t h_cause;
    uint8_t has_error = 0;
    const char *uuid;

    jss = js_mallocz(ctx, sizeof(js_session_t));
    if(!jss) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    if(argc > 0) {
        uuid = JS_ToCString(ctx, argv[0]);
        if(!strchr(uuid, '/')) {
            jss->session = switch_core_session_locate(uuid);
            jss->fl_hup_auto = SWITCH_TRUE;
        } else {
             if(argc > 1) {
                if(JS_IsObject(argv[1])) {
                    jss_old = JS_GetOpaque2(ctx, argv[1], js_session_class_id);
                }
                if(switch_ivr_originate((jss_old ? jss_old->session : NULL), &jss->session, &h_cause, uuid, 60, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) == SWITCH_STATUS_SUCCESS) {
                    jss->fl_hup_auto = SWITCH_TRUE;
                    switch_channel_set_state(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE);
                    switch_channel_wait_for_state_timeout(switch_core_session_get_channel(jss->session), CS_SOFT_EXECUTE, 5000);
                } else {
                    has_error = SWITCH_TRUE;
                    error = JS_ThrowTypeError(ctx, "Originate failed: %s", switch_channel_cause2str(h_cause));
                    goto fail;
                }
            }
            JS_FreeCString(ctx, uuid);
        }
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_session_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    jss->ctx = ctx;
    JS_SetOpaque(obj, jss);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-constructor: jss=%p, session=%p\n", jss, jss->session);

    return obj;
fail:
    if(jss) {
        js_free(ctx, jss);
    }
    JS_FreeValue(ctx, obj);
    return (has_error ? error : JS_EXCEPTION);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_seesion_class_get_id() {
    return js_session_class_id;
}

switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_session_class_id) {
        JS_NewClassID(&js_session_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_session_class_id, &js_session_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_session_proto_funcs, ARRAY_SIZE(js_session_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_session_contructor, CLASS_NAME, 2, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_session_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session) {
    js_session_t *jss;
    JSValue obj, proto;

    jss = js_mallocz(ctx, sizeof(js_session_t));
    if(!jss) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_session_proto_funcs, ARRAY_SIZE(js_session_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_session_class_id);
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    jss->ctx = ctx;
    jss->session = session;
    JS_SetOpaque(obj, jss);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-session-obj-created: jss=%p, session=%p\n", jss, jss->session);

    return obj;
}

JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss_a = NULL;
    js_session_t *jss_b = NULL;
    input_callback_state_t cb_state = { 0 };
    switch_input_args_t args = { 0 };
    switch_input_callback_function_t dtmf_func = NULL;
    int len = 0;
    void *bp = NULL;

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    jss_a = JS_GetOpaque(argv[0], js_session_class_id);
    if(!(jss_a && jss_a->session)) {
        return JS_ThrowTypeError(ctx, "Session A is not ready");
    }

    jss_b = JS_GetOpaque(argv[1], js_session_class_id);
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

        dtmf_func = js_collect_input_callback;
        bp = &cb_state;
        len = sizeof(cb_state);
    }

    switch_ivr_multi_threaded_bridge(jss_a->session, jss_b->session, dtmf_func, bp, bp);
    return JS_FALSE;
}
