/**
 * Session Class
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

static JSClassID js_session_class_id;

static void js_session_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_session_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    switch_caller_profile_t *caller_profile = NULL;

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }

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

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }
    return JS_ThrowTypeError(ctx, "Read only property");
}

static JSValue js_session_api_originate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);



    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_set_caller_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_set_hangup_hook(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_set_auto_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }

    if(argc >= 1) {
        int v = JS_ToBool(ctx, argv[0]);
        if(v < 0) {
            return JS_EXCEPTION;
        }
        jss->fl_hup = v;
    }
    return JS_TRUE;
}

static JSValue js_session_api_say_phrase(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *sess = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_stream_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_record_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_collect_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue  js_session_api_flush_events(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue  js_session_api_flush_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue  js_session_api_speak(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_set_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session ins't initialized");
    }

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

static JSValue js_session_api_get_var(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }

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

static JSValue js_session_api_get_digits(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;

    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }

    channel = switch_core_session_get_channel(jss->session);
    if(!switch_channel_ready(channel)) {
        return JS_ThrowTypeError(ctx, "Channel is not ready");
    }
    switch_channel_answer(channel);

    return JS_TRUE;
}

static JSValue js_session_api_pre_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);
    switch_channel_t *channel = NULL;
    if(!jss || !jss->session) {
        return JS_ThrowTypeError(ctx, "Session is not initialized");
    }

    channel = switch_core_session_get_channel(jss->session);
    if(!switch_channel_ready(channel)) {
        return JS_ThrowTypeError(ctx, "Channel is not ready");
    }
    switch_channel_pre_answer(channel);

    return JS_TRUE;
}

static JSValue js_session_api_generate_xml_cdr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_is_ready(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
}

static JSValue js_session_api_is_answered(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_test_flag(switch_core_session_get_channel(jss->session), CF_ANSWERED) ? JS_TRUE : JS_FALSE);
}

static JSValue js_session_api_is_media_ready(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    if(!jss || !jss->session) {
        return JS_FALSE;
    }

    return (switch_channel_media_ready(switch_core_session_get_channel(jss->session)) ? JS_TRUE : JS_FALSE);
}

static JSValue js_session_api_wait_for_answer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_wait_for_media(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_get_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_send_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_hangup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_session_api_sleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_session_t *jss = JS_GetOpaque2(ctx, this_val, js_session_class_id);

    return JS_ThrowTypeError(ctx, "Not yet implemented");
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
    JS_CFUNC_DEF("originate", 2, js_session_api_originate),
    JS_CFUNC_DEF("setCallerData", 2, js_session_api_set_caller_data),
    JS_CFUNC_DEF("setHangupHook", 1, js_session_api_set_hangup_hook),
    JS_CFUNC_DEF("setAutoHangup", 1, js_session_api_set_auto_hangup),
    JS_CFUNC_DEF("sayPhrase", 1, js_session_api_say_phrase),
    JS_CFUNC_DEF("streamFile", 1, js_session_api_stream_file),
    JS_CFUNC_DEF("recordFile", 1, js_session_api_record_file),
    JS_CFUNC_DEF("collectInput", 1, js_session_api_collect_input),
    JS_CFUNC_DEF("flushEvents", 1, js_session_api_flush_events),
    JS_CFUNC_DEF("flushDigits", 1, js_session_api_flush_digits),
    JS_CFUNC_DEF("speak", 1, js_session_api_speak),
    JS_CFUNC_DEF("setVariable", 2, js_session_api_set_var),
    JS_CFUNC_DEF("getVariable", 1, js_session_api_get_var),
    JS_CFUNC_DEF("getDigits", 1, js_session_api_get_digits),
    JS_CFUNC_DEF("answer", 0, js_session_api_answer),
    JS_CFUNC_DEF("preAnswer", 0, js_session_api_pre_answer),
    JS_CFUNC_DEF("generateXmlCdr", 0, js_session_api_generate_xml_cdr),
    JS_CFUNC_DEF("ready", 0, js_session_api_is_ready),
    JS_CFUNC_DEF("answered", 0, js_session_api_is_answered),
    JS_CFUNC_DEF("mediaReady", 0, js_session_api_is_media_ready),
    JS_CFUNC_DEF("waitForAnswer", 0, js_session_api_wait_for_answer),
    JS_CFUNC_DEF("waitForMedia", 0, js_session_api_wait_for_media),
    JS_CFUNC_DEF("getEvent", 0, js_session_api_get_event),
    JS_CFUNC_DEF("sendEvent", 0, js_session_api_send_event),
    JS_CFUNC_DEF("hangup", 0, js_session_api_hangup),
    JS_CFUNC_DEF("execute", 0, js_session_api_execute),
    JS_CFUNC_DEF("destroy", 0, js_session_api_destroy),
    JS_CFUNC_DEF("sleep", 1, js_session_api_sleep),
};

static void js_session_finalizer(JSRuntime *rt, JSValue val) {
    js_session_t *jss = JS_GetOpaque(val, js_session_class_id);

    if(jss->session) {
        switch_channel_t *channel = switch_core_session_get_channel(jss->session);
        //switch_channel_set_private(channel, "jss", NULL);
        //switch_core_event_hook_remove_state_change(session, hanguphook);
        if(jss->fl_hup) {
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
    if(JS_IsException(proto)) {
        goto fail;
    }
    obj = JS_NewObjectProtoClass(ctx, proto, js_session_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) {
        goto fail;
    }
    JS_SetOpaque(obj, jss);
    return obj;
fail:
    js_free(ctx, jss);
    JS_FreeValue(ctx, obj);
    return (has_error ? error : JS_EXCEPTION);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
void js_session_class_init_rt(JSRuntime *rt) {
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
