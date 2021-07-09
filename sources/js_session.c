/**
 * Session object
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define SESSION_CLASS_NAME                  "Session"
#define SESSION_PROP_NAME                   0
#define SESSION_PROP_UUID                   1
#define SESSION_PROP_STATE                  2
#define SESSION_PROP_CAUSE                  3
#define SESSION_PROP_CAUSECODE              4
#define SESSION_PROP_CALLER_ID_NAME         5
#define SESSION_PROP_CALLER_ID_NUMBER       6
#define SESSION_PROP_PROFILE_DIALPLAN       7
#define SESSION_PROP_PROFILE_DESTINATION    8

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
        JSValue function;
        JSValue arg;
        JSContext *ctx;
        js_session_t *jss;
} input_callback_state_t;

static JSClassID js_session_class_id;

static void js_session_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static switch_status_t js_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen);
static switch_status_t sys_session_hangup_hook(switch_core_session_t *session);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_session_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    switch_caller_profile_t *caller_profile = NULL;

    SESSION_SANITY_CHECK();

    channel = switch_core_session_get_channel(jss->session);
    caller_profile = switch_channel_get_caller_profile(channel);

    switch(magic) {
        case SESSION_PROP_NAME:
            return JS_NewString(ctx, switch_channel_get_name(channel));

        case SESSION_PROP_UUID:
            return JS_NewString(ctx, switch_channel_get_uuid(channel));

        case SESSION_PROP_STATE:
            return JS_NewString(ctx, switch_channel_state_name(switch_channel_get_state(channel)) );

        case SESSION_PROP_CAUSE:
            return JS_NewString(ctx, switch_channel_cause2str(switch_channel_get_cause(channel)) );

        case SESSION_PROP_CAUSECODE:
            return JS_NewInt32(ctx, switch_channel_get_cause(channel));

        case SESSION_PROP_CALLER_ID_NAME:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->caller_id_name);

        case SESSION_PROP_CALLER_ID_NUMBER:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->caller_id_number);

        case SESSION_PROP_PROFILE_DIALPLAN:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->dialplan);

        case SESSION_PROP_PROFILE_DESTINATION:
            if(!caller_profile) { return JS_UNDEFINED; }
            return JS_NewString(ctx, caller_profile->destination_number);
    }
    return JS_UNDEFINED;
}

static JSValue js_session_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Read only property!");
}

static JSValue js_session_originate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_set_caller_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_set_hangup_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    if(argc < 1 || !JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Callback function not defiend");
    }

    //jss->on_hangup = JS_DupValue(ctx, argv[0]);
    jss->on_hangup = argv[0];

    jss->hook_state = switch_channel_get_state(channel);
    switch_channel_set_private(channel, "jss", jss);
    switch_core_event_hook_add_state_change(jss->session, sys_session_hangup_hook);

    return JS_TRUE;
}

static JSValue js_session_set_auto_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    SESSION_SANITY_CHECK();

    if(argc >= 1) {
        int v = JS_ToBool(ctx, argv[0]);
        if(v < 0) { return JS_EXCEPTION; }
        jss->fl_hup = v;
    }
    return JS_TRUE;
}

static JSValue js_session_say_phrase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *sess = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_stream_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_record_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
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
            return JS_ThrowTypeError(ctx, "Callback function not defiend");
        }
        memset(&cb_state, 0, sizeof(cb_state));
        cb_state.ctx = ctx;
        cb_state.jss = jss;
        cb_state.arg = (argc > 1 ? argv[1] : JS_UNDEFINED);
        cb_state.function = JS_DupValue(ctx, argv[0]);

        dtmf_func = js_input_callback;
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

static JSValue  js_session_flush_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_event_t *event;

    SESSION_SANITY_CHECK();
    while(switch_core_session_dequeue_event(jss->session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
        switch_event_destroy(&event);
    }

    return JS_TRUE;
}

static JSValue  js_session_flush_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);

    switch_channel_flush_dtmf(channel);

    return JS_TRUE;
}

static JSValue  js_session_speak(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
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

static JSValue js_session_is_ready(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
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

static JSValue js_session_wait_for_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    switch_time_t started;
    unsigned int elapsed;
    uint32_t timeout = 60000;
    JSValue result;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    started = switch_micro_time_now();

    if(argc > 0) {
        JS_ToUint32(ctx, &timeout, argv[0]);
        if(timeout < 1000) { timeout = 1000; }
    }
    while(SWITCH_TRUE) {
        if(((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout) || switch_channel_down(channel)) {
            result = JS_FALSE;
            break;
        }
        if(switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_ANSWERED)) {
            result = JS_TRUE;
            break;
        }
        switch_cond_next();
    }
    return result;
}

static JSValue js_session_wait_for_media(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    switch_time_t started;
    unsigned int elapsed;
    uint32_t timeout = 60000;
    JSValue result;

    SESSION_SANITY_CHECK();
    channel = switch_core_session_get_channel(jss->session);
    CHANNEL_MEDIA_SANITY_CHECK();

    started = switch_micro_time_now();
    while(SWITCH_TRUE) {
        if(((elapsed = (unsigned int) ((switch_micro_time_now() - started) / 1000)) > (switch_time_t) timeout) || switch_channel_down(channel)) {
            result = JS_FALSE;
            break;
        }
        if(switch_channel_ready(channel) && (switch_channel_test_flag(channel, CF_ANSWERED) || switch_channel_test_flag(channel, CF_EARLY_MEDIA))) {
            result = JS_TRUE;
            break;
        }
        switch_cond_next();
    }
    return result;
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

        jss->hook_state = CS_HANGUP;
        //check_hangup_hook(jss);

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

    if(argc > 1) {
        if(JS_IsFunction(ctx, argv[1])) {
            memset(&cb_state, 0, sizeof(cb_state));
            cb_state.ctx = ctx;
            cb_state.jss = jss;
            cb_state.arg = (argc > 2 ? argv[2] : JS_UNDEFINED);
            cb_state.function = JS_DupValue(ctx, argv[1]);

            dtmf_func = js_input_callback;
            bp = &cb_state;
            len = sizeof(cb_state);
        }
    }

    args.input_callback = dtmf_func;
    args.buf = bp;
    args.buflen = len;

    switch_ivr_sleep(jss->session, msec, SWITCH_FALSE, &args);

    if(len > 0) {
        JS_FreeValue(ctx, cb_state.function);
    }

    return JS_TRUE;
}

static JSValue js_session_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return js_session_hangup(ctx, this_val, argc, argv);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// callbacks
static switch_status_t sys_session_hangup_hook(switch_core_session_t *session) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_channel_state_t state = switch_channel_get_state(channel);
    js_session_t *jss = NULL;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "sys_hanguphook: state=%i\n", (int) state);

    if(state == CS_HANGUP || state == CS_ROUTING) {
        if((jss = switch_channel_get_private(channel, "jss"))) {
            jss->hook_state = state;
            //jss->check_state = 0;
        }
    }
    return SWITCH_STATUS_SUCCESS;
}

static switch_status_t js_input_callback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen) {
    switch_status_t status;
    input_callback_state_t *cb_state = buf;
    js_session_t *jss = cb_state->jss;
    JSContext *ctx = cb_state->ctx;
    JSValue args[3] = { 0 };
    JSValue ret_val;

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js_input_callback: itype=%i, jss=%p\n", (int) itype, jss);

    switch(itype) {
        case SWITCH_INPUT_TYPE_EVENT: {
            //
            // todo
            //
            break;
        }
        case SWITCH_INPUT_TYPE_DTMF: {
            args[0] = js_dtmf_object_create(ctx, (switch_dtmf_t *) input);
            args[1] = JS_NewString(ctx, "dtmf");
            args[2] = cb_state->arg;

            ret_val = JS_Call(ctx, cb_state->function, JS_UNDEFINED, 3, (JSValueConst *) args);
            if(JS_IsException(ret_val)) {
                ctx_dump_error(NULL, NULL, ctx);
            }

            JS_FreeValue(ctx, args[0]);
            JS_FreeValue(ctx, args[1]);
            JS_FreeValue(ctx, ret_val);
            break;
        }
    }
    return SWITCH_STATUS_SUCCESS;
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_session_class = {
    SESSION_CLASS_NAME,
    .finalizer = js_session_finalizer,
};

static const JSCFunctionListEntry js_session_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", js_session_property_get, js_session_property_set, SESSION_PROP_NAME),
    JS_CGETSET_MAGIC_DEF("uuid", js_session_property_get, js_session_property_set, SESSION_PROP_UUID),
    JS_CGETSET_MAGIC_DEF("state", js_session_property_get, js_session_property_set, SESSION_PROP_STATE),
    JS_CGETSET_MAGIC_DEF("cause", js_session_property_get, js_session_property_set, SESSION_PROP_CAUSE),
    JS_CGETSET_MAGIC_DEF("causecode", js_session_property_get, js_session_property_set, SESSION_PROP_CAUSECODE),
    JS_CGETSET_MAGIC_DEF("dialplan", js_session_property_get, js_session_property_set, SESSION_PROP_PROFILE_DIALPLAN),
    JS_CGETSET_MAGIC_DEF("destination", js_session_property_get, js_session_property_set, SESSION_PROP_PROFILE_DESTINATION),
    JS_CGETSET_MAGIC_DEF("caller_id_name", js_session_property_get, js_session_property_set, SESSION_PROP_CALLER_ID_NAME),
    JS_CGETSET_MAGIC_DEF("caller_id_number", js_session_property_get, js_session_property_set, SESSION_PROP_CALLER_ID_NUMBER),
    //
    JS_CFUNC_DEF("originate", 2, js_session_originate),
    JS_CFUNC_DEF("setCallerData", 2, js_session_set_caller_data),
    JS_CFUNC_DEF("setHangupHook", 1, js_session_set_hangup_hook),       // ok
    JS_CFUNC_DEF("setAutoHangup", 1, js_session_set_auto_hangup),       // ok
    JS_CFUNC_DEF("sayPhrase", 1, js_session_say_phrase),
    JS_CFUNC_DEF("streamFile", 1, js_session_stream_file),
    JS_CFUNC_DEF("recordFile", 1, js_session_record_file),
    JS_CFUNC_DEF("collectInput", 1, js_session_collect_input),          // ok
    JS_CFUNC_DEF("flushEvents", 1, js_session_flush_events),            // ok
    JS_CFUNC_DEF("flushDigits", 1, js_session_flush_digits),            // ok
    JS_CFUNC_DEF("speak", 1, js_session_speak),
    JS_CFUNC_DEF("setVariable", 2, js_session_set_var),                 // ok
    JS_CFUNC_DEF("getVariable", 1, js_session_get_var),                 // ok
    JS_CFUNC_DEF("getDigits", 4, js_session_get_digits),                // ok
    JS_CFUNC_DEF("answer", 0, js_session_answer),                       // ok
    JS_CFUNC_DEF("preAnswer", 0, js_session_pre_answer),                // ok
    JS_CFUNC_DEF("generateXmlCdr", 0, js_session_generate_xml_cdr),     // ok
    JS_CFUNC_DEF("ready", 0, js_session_is_ready),                      // ok
    JS_CFUNC_DEF("answered", 0, js_session_is_answered),                // ok
    JS_CFUNC_DEF("mediaReady", 0, js_session_is_media_ready),           // ok
    JS_CFUNC_DEF("waitForAnswer", 0, js_session_wait_for_answer),       // ok
    JS_CFUNC_DEF("waitForMedia", 0, js_session_wait_for_media),         // ok
    JS_CFUNC_DEF("getEvent", 0, js_session_get_event),                  // ok
    JS_CFUNC_DEF("sendEvent", 0, js_session_send_event),                // ok
    JS_CFUNC_DEF("hangup", 0, js_session_hangup),                       // ok
    JS_CFUNC_DEF("execute", 0, js_session_execute),                     // ok
    JS_CFUNC_DEF("sleep", 1, js_session_sleep),                         // ok
    JS_CFUNC_DEF("destroy", 0, js_session_destroy),                     // ok
};

static void js_session_finalizer(JSRuntime *rt, JSValue val) {
    js_session_t *jss = JS_GetOpaque(val, js_session_class_id);

    if(!jss) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-session-finalizer: jss=%p, session=%p\n", jss, jss->session);

    if(jss->session) {
        switch_channel_t *channel = switch_core_session_get_channel(jss->session);

        switch_channel_set_private(channel, "jss", NULL);
        switch_core_event_hook_remove_state_change(jss->session, sys_session_hangup_hook);

        if(jss->fl_hup) {
            switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
        }

        //JS_FreeValue(ctx, jss->on_hangup);

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
            jss->fl_hup = SWITCH_TRUE;
        } else {
             if(argc > 1) {
                if(JS_IsObject(argv[1])) {
                    jss_old = JS_GetOpaque2(ctx, argv[1], js_session_class_id);
                }
                if(switch_ivr_originate((jss_old ? jss_old->session : NULL), &jss->session, &h_cause, uuid, 60, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) == SWITCH_STATUS_SUCCESS) {
                    jss->fl_hup = SWITCH_TRUE;
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

    JS_SetOpaque(obj, jss);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-session-constructor: jss=%p, session=%p\n", jss, jss->session);

    return obj;
fail:
    js_free(ctx, jss);
    JS_FreeValue(ctx, obj);
    return (has_error ? error : JS_EXCEPTION);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_seesion_class_get_id() {
    return js_session_class_id;
}

void js_session_class_register_rt(JSRuntime *rt) {
    JS_NewClassID(&js_session_class_id);
    JS_NewClass(rt, js_session_class_id, &js_session_class);
}

switch_status_t js_session_class_register_ctx(JSContext *ctx, JSValue global_obj) {
    JSValue session_proto;
    JSValue session_class;

    session_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, session_proto, js_session_proto_funcs, ARRAY_SIZE(js_session_proto_funcs));

    session_class = JS_NewCFunction2(ctx, js_session_contructor, SESSION_CLASS_NAME, 2, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, session_class, session_proto);
    JS_SetClassProto(ctx, js_session_class_id, session_proto);

    JS_SetPropertyStr(ctx, global_obj, SESSION_CLASS_NAME, session_class);
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

    jss->session = session;
    JS_SetOpaque(obj, jss);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-session-obj-created: jss=%p, session=%p\n", jss, jss->session);

    return obj;
}
