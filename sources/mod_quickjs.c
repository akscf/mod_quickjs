/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

static struct {
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_scripts;
    switch_inthash_t        *scripts_map;
    switch_mutex_t          *mutex_qjsrt;
    JSRuntime               *qjs_rt;
    uint8_t                 fl_ready;
    uint8_t                 fl_shutdown;
    int                     active_threads;
} globals;

SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown);
SWITCH_MODULE_DEFINITION(mod_quickjs, mod_quickjs_load, mod_quickjs_shutdown, NULL);

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj);

static script_t *script_lookup(char *name);
static uint32_t script_sem_take(script_t *script);
static void script_sem_release(script_t *script);
static uint32_t script_instance_sem_take(script_instance_t *instance);
static void script_instance_sem_release(script_instance_t *instance);

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
/* Session class */
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

typedef struct {
    switch_core_session_t   *session;
    uint8_t                 flags_hup;
} js_session_t;

static void js_session_finalizer(JSRuntime *rt, JSValue val) {
    js_session_t *jss = JS_GetOpaque(val, js_session_class_id);

    if(jss->session) {
        //switch_channel_t *channel = switch_core_session_get_channel(jss->session);
        //switch_channel_set_private(channel, "jss", NULL);
        //switch_core_event_hook_remove_state_change(session, hanguphook);
        //if(switch_test_flag(jss, S_HUP)) {
        //    switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
        //}
        switch_core_session_rwunlock(jss->session);
    }
    js_free_rt(rt, jss);
}

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
        jss->flags_hup = v;
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

static JSValue js_session_api_generate_xml_xdr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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

// ---------
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
    JS_CFUNC_DEF("generateXmlCdr", 0, js_session_api_generate_xml_xdr),
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

static JSValue js_session_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    js_session_t *jss = NULL;
    const char *uuid;

    jss = js_mallocz(ctx, sizeof(js_session_t));
    if(!jss) {
        return JS_EXCEPTION;
    }
    // -------------------------------------------------------------
    /* create a session */
    uuid = JS_ToCString(ctx, argv[0]);
    if(zstr(uuid)) {
        goto fail;
    }
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js_session_contructor: session-uuid=%s\n", uuid);
    JS_FreeCString(ctx, uuid);

    // -------------------------------------------------------------
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
    return JS_EXCEPTION;
}

static void js_session_class_init_rt(JSRuntime *rt) {
    JS_NewClassID(&js_session_class_id);
    JS_NewClass(rt, js_session_class_id, &js_session_class);
}

static switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj) {
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

// ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    switch_log_level_t level = SWITCH_LOG_DEBUG;
    const char *file = __FILE__;
    int line = __LINE__;

    if(argc > 1) {
        const char *lvl_str, *msg_str;
        lvl_str = JS_ToCString(ctx, argv[0]);
        if(!zstr(lvl_str)) {
            level = switch_log_str2level(lvl_str);
        }
        if(level == SWITCH_LOG_INVALID) {
            level = SWITCH_LOG_DEBUG;
        }

        msg_str = JS_ToCString(ctx, argv[1]);
        switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s\n", msg_str);

        JS_FreeCString(ctx, lvl_str);
        JS_FreeCString(ctx, msg_str);
    } else if(argc > 0) {
        const char *msg_str;
        msg_str = JS_ToCString(ctx, argv[0]);

        if(!zstr(msg_str)) {
            switch_log_printf(SWITCH_CHANNEL_ID_LOG, file, "console_log", line, NULL, level, "%s\n", msg_str);
        }

        JS_FreeCString(ctx, msg_str);
    }
    return JS_UNDEFINED;
}

static JSValue js_msleep(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    uint32_t msec = 0;

    if(argc > 0) {
        JS_ToUint32(ctx, &msec, argv[0]);
    }
    if(msec) {
        switch_yield(msec * 1000);
        return JS_TRUE;
    }
    return JS_FALSE;
}

static JSValue js_global_set(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if(argc >= 2) {
        const char *var_str, *val_str;

        var_str = JS_ToCString(ctx, argv[0]);
        val_str = JS_ToCString(ctx, argv[1]);

        switch_core_set_variable(var_str, val_str);

        JS_FreeCString(ctx, var_str);
        JS_FreeCString(ctx, val_str);

        return JS_TRUE;
    }

    return JS_FALSE;
}

static JSValue js_global_get(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if(argc >= 1) {
        const char *var_str;
        char *val = NULL;

        var_str = JS_ToCString(ctx, argv[0]);
        val = switch_core_get_variable(var_str);
        JS_FreeCString(ctx, var_str);

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

static JSValue js_exit(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue ret_val;
    const char *exit_code;

    if(!argc) {
        return JS_FALSE;
    }

    exit_code = JS_ToCString(ctx, argv[0]);
    if(zstr(exit_code)) {
        JS_FreeCString(ctx, exit_code);
        return JS_FALSE;
    }

    ret_val = JS_ThrowTypeError(ctx, "ERROR: %s", exit_code);
    JS_FreeCString(ctx, exit_code);

    return ret_val;
}

static JSValue js_system(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *cmd;
    int result;

    if(!argc) {
        return JS_FALSE;
    }

    cmd = JS_ToCString(ctx, argv[0]);
    if(zstr(cmd)) {
        return JS_UNDEFINED;
    }
    result = switch_system(cmd, SWITCH_TRUE);
    JS_FreeCString(ctx, cmd);

    return JS_NewInt32(ctx, result);
}

static JSValue js_api_execute(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    const char *api_str;
    const char *arg_str;
    script_instance_t *script_instance = NULL;
    switch_core_session_t *session = NULL;
    switch_stream_handle_t stream = { 0 };
    JSValue js_ret_val;

    if(argc < 1) {
        return JS_FALSE;
    }

    api_str = JS_ToCString(ctx, argv[0]);
    arg_str = (argc > 1 ? JS_ToCString(ctx, argv[1]) : NULL);

    if(zstr(api_str)) {
        return JS_UNDEFINED;
    }

    script_instance = JS_GetContextOpaque(ctx);
    session = (script_instance ? script_instance->session : NULL);

    SWITCH_STANDARD_STREAM(stream);
    switch_api_execute(api_str, arg_str, session, &stream);
    js_ret_val = JS_NewString(ctx, switch_str_nil((char *) stream.data));

    switch_safe_free(stream.data);
    if(api_str) JS_FreeCString(ctx, api_str);
    if(arg_str) JS_FreeCString(ctx, arg_str);

    return js_ret_val;
}

static JSValue js_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    void *bp = NULL;
    switch_input_callback_function_t dtmf_func = NULL;
    js_session_t *jss_a = NULL, *jss_b = NULL;
    JSValue js_cb_fnc;

    if(argc >= 2) {
        jss_a = JS_GetOpaque(argv[0], js_session_class_id);
        jss_b = JS_GetOpaque(argv[1], js_session_class_id);

        if(!(jss_a && jss_a->session)) {
            return JS_ThrowTypeError(ctx, "session A is not ready");
        }
        if(!(jss_b && jss_b->session)) {
            return JS_ThrowTypeError(ctx, "session B is not ready");
        }

        if(argc >= 3) {
            if(JS_IsFunction(ctx, argv[2])) {
                // js_cb_fnc = JS_DupValue(ctx, argv[2]);
            }
        }

        switch_ivr_multi_threaded_bridge(jss_a->session, jss_b->session, dtmf_func, bp, bp);
    }
    return JS_FALSE;
}

static void ctx_dump_error(script_t *script, script_instance_t *instance, JSContext *ctx) {
    JSValue exception_val = JS_GetException(ctx);

    if(JS_IsError(ctx, exception_val)) {
        JSValue stk_val = JS_GetPropertyStr(ctx, exception_val, "stack");
        const char *err_str = JS_ToCString(ctx, exception_val);
        const char *stk_str = (JS_IsUndefined(stk_val) ? NULL : JS_ToCString(ctx, stk_val));

        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "(%X:%s)\n%s %s\n", instance->id, script->name, err_str, stk_str);

        if(err_str) JS_FreeCString(ctx, err_str);
        if(stk_str) JS_FreeCString(ctx, stk_str);

        JS_FreeValue(ctx, stk_val);
    }

    JS_FreeValue(ctx, exception_val);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static uint32_t name2id(char *name, uint32_t len) {
    return switch_crc32_8bytes((char *)name, len);
}

static uint32_t rand_id() {
    char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
    memset((char *)uuid_str, '\0' , sizeof(uuid_str));
    switch_uuid_str(uuid_str, sizeof(uuid_str));
    return name2id((char *)uuid_str, strlen(uuid_str));
}

void launch_thread(switch_memory_pool_t *pool, switch_thread_start_t fun, void *data) {
    switch_threadattr_t *attr = NULL;
    switch_thread_t *thread = NULL;

    switch_threadattr_create(&attr, pool);
    switch_threadattr_detach_set(attr, 1);
    switch_threadattr_stacksize_set(attr, SWITCH_THREAD_STACKSIZE);
    switch_thread_create(&thread, attr, fun, data, pool);

    switch_mutex_lock(globals.mutex);
    globals.active_threads++;
    switch_mutex_unlock(globals.mutex);

    return;
}

static script_t *script_lookup(char *name) {
    script_t *script = NULL;
    int32_t id = 0;

    if(zstr(name) || globals.fl_shutdown) {
        return NULL;
    }
    id = name2id(name, strlen(name));

    switch_mutex_lock(globals.mutex_scripts);
    script = switch_core_inthash_find(globals.scripts_map, id);
    switch_mutex_unlock(globals.mutex_scripts);

    return script;
}

static script_instance_t *instance_lookup(script_t *script, uint32_t id) {
    script_instance_t *instance = NULL;

    if(!script || globals.fl_shutdown) {
        return NULL;
    }
    if(script_sem_take(script)) {
        switch_mutex_lock(script->mutex);
        instance = switch_core_inthash_find(script->instances_map, id);
        switch_mutex_unlock(script->mutex);
        script_sem_release(script);
    }
    return instance;
}

static uint32_t script_sem_take(script_t *script) {
    uint32_t status = SWITCH_FALSE;

    if(!script) { return SWITCH_FALSE; }

    switch_mutex_lock(script->mutex);
    if(script->fl_ready) {
        status = SWITCH_TRUE;
        script->tx_sem++;
    }
    switch_mutex_unlock(script->mutex);
    return status;
}

static void script_sem_release(script_t *script) {
    switch_assert(script);

    switch_mutex_lock(script->mutex);
    if(script->tx_sem) {
        script->tx_sem--;
    }
    switch_mutex_unlock(script->mutex);
}

static uint32_t script_instance_sem_take(script_instance_t *instance) {
    uint32_t status = SWITCH_FALSE;

    if(!instance) { return SWITCH_FALSE; }

    switch_mutex_lock(instance->mutex);
    if(instance->fl_ready) {
        status = SWITCH_TRUE;
        instance->tx_sem++;
    }
    switch_mutex_unlock(instance->mutex);
    return status;
}

static void script_instance_sem_release(script_instance_t *instance) {
    switch_assert(instance);

    switch_mutex_lock(instance->mutex);
    if(instance->tx_sem) {
        instance->tx_sem--;
    }
    switch_mutex_unlock(instance->mutex);
}

static switch_status_t script_load_code(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_file_t *file = NULL;

    switch_assert(script);

    if((status = switch_file_open(&file, script->path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, script->pool)) != SWITCH_STATUS_SUCCESS) {
        return status;
    }
    script->code_length = switch_file_get_size(file);
    if(!script->code_length) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }
    if((script->code = switch_core_alloc(script->pool, script->code_length + 1)) == NULL)  {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        status = SWITCH_STATUS_MEMERR;
        goto out;
    }
    if(switch_file_read(file, script->code, &script->code_length) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Couldn't read file\n");
        return SWITCH_STATUS_GENERR;
    }
    script->code[script->code_length] = '\0';
out:
    switch_file_close(file);
    return status;
}

static switch_status_t script_setup_ctx(script_t *script, script_instance_t *script_instance) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    JSContext *ctx = script_instance->ctx;
    JSValue global_obj, session_obj, argc_obj, argv_obj;
    switch_channel_t *channel = NULL;
    switch_caller_profile_t *caller_profile = NULL;

    /* init */
    JS_SetContextOpaque(ctx, script_instance);

    /* global objects */
    global_obj = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global_obj, "script_id", JS_NewInt32(ctx, script_instance->id));
    js_session_class_register(ctx, global_obj);

    /* script args */
    if(!zstr(script_instance->args)) {
        char *argv[512];
        int argc = 0;

        argc = switch_separate_string(script_instance->args, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
        argc_obj = JS_NewInt32(ctx, argc);
        argv_obj = JS_NewArray(ctx);

        if(argc) {
            for (int i = 0; i < argc; i++) {
                JS_SetPropertyUint32(ctx, argv_obj, (uint32_t) i, JS_NewString(ctx, argv[i]));
            }
        }
        JS_SetPropertyStr(ctx, global_obj, "argc", argc_obj);
        JS_SetPropertyStr(ctx, global_obj, "argv", argv_obj);
    } else {
        JS_SetPropertyStr(ctx, global_obj, "argc", JS_NewInt32(ctx, 0));
        JS_SetPropertyStr(ctx, global_obj, "argv", JS_NewArray(ctx));
    }

    /* global api */
    JS_SetPropertyStr(ctx, global_obj, "console_log", JS_NewCFunction(ctx, js_console_log, "console_log", 0));
    JS_SetPropertyStr(ctx, global_obj, "msleep", JS_NewCFunction(ctx, js_msleep, "msleep", 1));
    JS_SetPropertyStr(ctx, global_obj, "bridge", JS_NewCFunction(ctx, js_bridge, "bridge", 2));
    JS_SetPropertyStr(ctx, global_obj, "system", JS_NewCFunction(ctx, js_system, "system", 1));
    JS_SetPropertyStr(ctx, global_obj, "exit", JS_NewCFunction(ctx, js_exit, "exit", 0));
    JS_SetPropertyStr(ctx, global_obj, "apiExecute", JS_NewCFunction(ctx, js_api_execute, "apiExecute", 2));
    JS_SetPropertyStr(ctx, global_obj, "setGlobalVariable", JS_NewCFunction(ctx, js_global_set, "setGlobalVariable", 2));
    JS_SetPropertyStr(ctx, global_obj, "getGlobalVariable", JS_NewCFunction(ctx, js_global_get, "getGlobalVariable", 2));

    /* session */
    if(script_instance->session) {
        /*session_obj = JS_NewObject(ctx);
        channel = switch_core_session_get_channel(script_instance->session);
        caller_profile = switch_channel_get_caller_profile(channel);
        //
        JS_SetPropertyStr(ctx, global_obj, "session", session_obj);*/
    }

    JS_FreeValue(ctx, global_obj);
    return status;
}

static switch_status_t script_destroy(script_t *script) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;

    if(script_sem_take(script)) {
        script->fl_ready = SWITCH_FALSE;
        script->fl_destroyed = SWITCH_TRUE;

        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_delete(globals.scripts_map, script->id);
        switch_mutex_unlock(globals.mutex_scripts);

        while(script->tx_sem > 1) {
            switch_yield(100000);
        }

        switch_core_destroy_memory_pool(&script->pool);
    }
    return status;
}

static switch_status_t script_launch(switch_core_session_t *session, char *script_name, char *script_args, uint8_t async) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *script_path_local = NULL;
    char *script_args_local = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_memory_pool_t *ipool = NULL;
    script_t *script = NULL;
    script_instance_t *script_instance = NULL;
    uint32_t id = 0;

    if(zstr(script_name)) {
        status = SWITCH_STATUS_FALSE;
        goto out;
    }

    id = name2id(script_name, strlen(script_name));
    if(!zstr(script_args)) {
        script_args_local = strdup(script_args);
    }

    if(switch_file_exists(script_name, NULL) == SWITCH_STATUS_SUCCESS) {
        script_path_local = strdup(script_name);
    } else {
        script_path_local = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.script_dir, SWITCH_PATH_SEPARATOR, script_name);
        if(switch_file_exists(script_path_local, NULL) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "File not found: %s\n", script_path_local);
            status = SWITCH_STATUS_FALSE;
            goto out;
        }
    }

    script = script_lookup(script_name);
    if(!script) {
        if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        if((script = switch_core_alloc(pool, sizeof(script_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        script->id = id;
        script->pool = pool;
        script->path = switch_core_strdup(pool, script_path_local);
        script->name = basename(script->path);

        switch_core_inthash_init(&script->instances_map);
        switch_mutex_init(&script->mutex, SWITCH_MUTEX_NESTED, pool);

        /* load script */
        if((status = script_load_code(script)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't load script\n");
            goto out;
        }

        /* add to map */
        switch_mutex_lock(globals.mutex_scripts);
        switch_core_inthash_insert(globals.scripts_map, script->id, script);
        switch_mutex_unlock(globals.mutex_scripts);

        script->fl_ready = SWITCH_TRUE;
    }

    if(script_sem_take(script)) {
        if(switch_core_new_memory_pool(&ipool) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        if((script_instance = switch_core_alloc(ipool, sizeof(script_instance_t))) == NULL)  {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
            status = SWITCH_STATUS_MEMERR;
            goto out;
        }
        script_instance->pool = ipool;
        script_instance->script = script;
        script_instance->session = session;
        script_instance->session_id = (session ? switch_core_session_get_uuid(session) : NULL);
        script_instance->args = (zstr(script_args_local) ? NULL : switch_core_strdup(ipool, script_args_local));
        script_instance->id = (zstr(script_instance->session_id) ? rand_id() : name2id((char *)script_instance->session_id, strlen(script_instance->session_id)));
        script_instance->fl_async = async;
        switch_mutex_init(&script_instance->mutex, SWITCH_MUTEX_NESTED, ipool);

        /* ctx */
        switch_mutex_lock(globals.mutex_qjsrt);
        script_instance->ctx = JS_NewContext(globals.qjs_rt);
        switch_mutex_unlock(globals.mutex_qjsrt);

        if(!script_instance->ctx) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create ctx (JS_NewContext fail)\n");
            status = SWITCH_STATUS_FALSE;
            goto out;
        }

        if(script_setup_ctx(script, script_instance) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't setup ctx\n");
            status = SWITCH_STATUS_FALSE;
            goto out;
        }

        switch_mutex_lock(script->mutex);
        switch_core_inthash_insert(script->instances_map, script_instance->id, script_instance);
        script->instances++;
        switch_mutex_unlock(script->mutex);

        script_sem_take(script);
        launch_thread(ipool, script_instance_thread, script_instance);

        if(!async) {
            while(SWITCH_TRUE) {
                if(script->fl_do_kill || script_instance->fl_do_kill || script->fl_destroyed || script_instance->fl_destroyed || globals.fl_shutdown) {
                    break;
                }
                switch_yield(10000);
            }
            script_instance->fl_ready = SWITCH_FALSE;
            script_instance->fl_destroyed = SWITCH_TRUE;

            switch_mutex_lock(script->mutex);
            switch_core_inthash_delete(script->instances_map, script_instance->id);
            script->instances--;
            switch_mutex_unlock(script->mutex);

            while(script_instance->tx_sem > 0) {
                switch_yield(100000);
            }

            switch_mutex_lock(script_instance->mutex);
            if(script_instance->ctx) {
                JS_FreeContext(script_instance->ctx);
            }
            switch_mutex_unlock(script_instance->mutex);

            switch_core_destroy_memory_pool(&script_instance->pool);
        }
        script_sem_release(script);

        if(!script->instances) {
            script_destroy(script);
        }
    } else {
        status = SWITCH_STATUS_FALSE;
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(pool)  { switch_core_destroy_memory_pool(&pool); }
        if(ipool) { switch_core_destroy_memory_pool(&ipool); }
    }
    switch_safe_free(script_path_local);
    switch_safe_free(script_args_local);
    return status;
}

static void *SWITCH_THREAD_FUNC script_instance_thread(switch_thread_t *thread, void *obj) {
    volatile script_instance_t *_ref = (script_instance_t *) obj;
    script_instance_t *script_instance = (script_instance_t *) _ref;
    script_t *script = (script_t *)script_instance->script;
    uint8_t fl_async = script_instance->fl_async;
    JSValue result;

    if(script->fl_do_kill || script_instance->fl_do_kill || script->fl_destroyed || script_instance->fl_destroyed || globals.fl_shutdown) {
        goto out;
    }

    script_instance->fl_ready = SWITCH_TRUE;
    result = JS_Eval(script_instance->ctx, script->code, script->code_length, script->name, JS_EVAL_TYPE_GLOBAL | JS_EVAL_TYPE_MODULE);
    if(JS_IsException(result)) {
        ctx_dump_error(script, script_instance, script_instance->ctx);
        goto out;
    }
    JS_FreeValue(script_instance->ctx, result);

out:
    script_instance->fl_ready = SWITCH_FALSE;
    script_instance->fl_destroyed = SWITCH_TRUE;

    if(fl_async) {
        switch_mutex_lock(script->mutex);
        switch_core_inthash_delete(script->instances_map, script_instance->id);
        script->instances--;
        switch_mutex_unlock(script->mutex);

        while(script_instance->tx_sem > 0) {
            switch_yield(100000);
        }

        switch_mutex_lock(script_instance->mutex);
        if(script_instance->ctx) {
            JS_FreeContext(script_instance->ctx);
        }
        switch_mutex_unlock(script_instance->mutex);

        switch_core_destroy_memory_pool(&script_instance->pool);
    }
    script_sem_release(script);

    if(fl_async && !script->instances) {
        script_destroy(script);
    }

    switch_mutex_lock(globals.mutex);
    if(globals.active_threads > 0) globals.active_threads--;
    switch_mutex_unlock(globals.mutex);

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
static void event_handler_shutdown(switch_event_t *event) {
    globals.fl_shutdown = SWITCH_TRUE;
}

#define CMD_SYNTAX "\n" \
    "list - show running scripts\n" \
    "run scriptName [args] - launch a new script instance\n"

SWITCH_STANDARD_API(quickjs_cmd) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[5] = { 0 };
    int argc = 0;

    if(!zstr(cmd)) {
        mycmd = strdup(cmd);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(argc == 0) { goto usage; }
    if(globals.fl_shutdown) { goto out; }

    if(argc == 1) {
        if(strcasecmp(argv[0], "list") == 0) {
            switch_hash_index_t *hidx = NULL;
            switch_hash_index_t *hidx2 = NULL;

            switch_mutex_lock(globals.mutex_scripts);
            for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
                void *hval = NULL;
                script_t *script = NULL;

                switch_core_hash_this(hidx, NULL, NULL, &hval);
                script = (script_t *)hval;

                if(script_sem_take(script)) {
                    stream->write_function(stream, "%s [%i] (%s)\n", script->name, script->instances, script->path);
                    for(hidx2 = switch_core_hash_first_iter(script->instances_map, hidx2); hidx2; hidx2 = switch_core_hash_next(&hidx2)) {
                        script_instance_t *script_instance = NULL;
                        void *hval2 = NULL;

                        switch_core_hash_this(hidx2, NULL, NULL, &hval2);
                        script_instance = (script_instance_t *)hval2;

                        if(script_instance_sem_take(script_instance)) {
                            stream->write_function(stream, " - %X (session: %s)\n", script_instance->id, (script_instance->session_id ? script_instance->session_id : "none"));
                            script_instance_sem_release(script_instance);
                        }
                    }
                    script_sem_release(script);
                }
            }
            switch_mutex_unlock(globals.mutex_scripts);
            goto out;
        }
        goto usage;
    }
    if(strcasecmp(argv[0], "run") == 0) {
        char *script_args = (argc > 2 ? ((char *)cmd + (strlen(argv[0]) + strlen(argv[1]) + 2)) : NULL);

        if((status = script_launch(session, argv[1], script_args, SWITCH_TRUE)) == SWITCH_STATUS_SUCCESS) {
            stream->write_function(stream, "+OK\n");
        } else {
            stream->write_function(stream, "-ERR: %i\n", status);
        }
        goto out;
    }
    goto out;
usage:
    stream->write_function(stream, "-USAGE: %s\n", CMD_SYNTAX);
out:
    switch_safe_free(mycmd);
    return SWITCH_STATUS_SUCCESS;
}

#define APP_SYNTAX "scriptName [args]"
SWITCH_STANDARD_APP(quickjs_app) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    char *mycmd = NULL, *argv[2] = { 0 };
    char *script_name = NULL, *script_args = NULL;
    int argc = 0;

    if(!zstr(data)) {
        mycmd = strdup(data);
        switch_assert(mycmd);
        argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
    }
    if(globals.fl_shutdown) { goto out; }
    if(argc < 1) { goto usage; }

    script_name = argv[0];
    script_args = (argc > 1 ? ((char *)data + (strlen(argv[0]) + 1)) : NULL);

    if((status = script_launch(NULL, script_name, script_args, SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't launch: %s\n", script_name);
    }

    goto out;
usage:
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%s\n", APP_SYNTAX);
out:
    switch_safe_free(mycmd);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------------------------------------------------------------------------
#define CONFIG_NAME "quickjs.conf"
SWITCH_MODULE_LOAD_FUNCTION(mod_quickjs_load) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_xml_t cfg = NULL, xml = NULL, settings = NULL, param = NULL;
    switch_api_interface_t *cmd_interface;
    switch_application_interface_t *app_interface;

    memset(&globals, 0, sizeof (globals));
    switch_core_inthash_init(&globals.scripts_map);
    switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_scripts, SWITCH_MUTEX_NESTED, pool);
    switch_mutex_init(&globals.mutex_qjsrt, SWITCH_MUTEX_NESTED, pool);

    /* quickjs rt */
    globals.qjs_rt = JS_NewRuntime();
    if(!globals.qjs_rt) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't create qjs runtime (JS_NewRuntime fail)\n");
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    JS_SetCanBlock(globals.qjs_rt, 1);
    JS_SetRuntimeInfo(globals.qjs_rt, "mod_quickjs");
    js_session_class_init_rt(globals.qjs_rt);

    /* xml config */
    if((xml = switch_xml_open_cfg(CONFIG_NAME, &cfg, NULL)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open: %s\n", CONFIG_NAME);
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }
    if((settings = switch_xml_child(cfg, "settings"))) {
        for(param = switch_xml_child(settings, "param"); param; param = param->next) {
            char *var = (char *) switch_xml_attr_soft(param, "name");
            char *val = (char *) switch_xml_attr_soft(param, "value");

        }
    }

    *module_interface = switch_loadable_module_create_module_interface(pool, modname);
    SWITCH_ADD_API(cmd_interface, "qjs", "manage quickjs scripts", quickjs_cmd, CMD_SYNTAX);
    SWITCH_ADD_APP(app_interface, "qjs", "manage quickjs scripts", "manage quickjs scripts", quickjs_app, APP_SYNTAX, SAF_NONE);

    if (switch_event_bind(modname, SWITCH_EVENT_SHUTDOWN, SWITCH_EVENT_SUBCLASS_ANY, event_handler_shutdown, NULL) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event handler!\n");
        switch_goto_status(SWITCH_STATUS_GENERR, done);
    }

    globals.fl_shutdown = SWITCH_FALSE;
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "quckjs (%s)\n", MOD_VERSION);

done:
    if(xml) {
        switch_xml_free(xml);
    }
    return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_quickjs_shutdown) {
    switch_hash_index_t *hidx = NULL;
    void *hval = NULL;

    switch_event_unbind_callback(event_handler_shutdown);

    globals.fl_shutdown = SWITCH_TRUE;
    while(globals.active_threads > 0) {
        switch_yield(100000);
    }

    switch_mutex_lock(globals.mutex_scripts);
    for(hidx = switch_core_hash_first_iter(globals.scripts_map, hidx); hidx; hidx = switch_core_hash_next(&hidx)) {
        script_t *script = NULL;

        switch_core_hash_this(hidx, NULL, NULL, &hval);
        script = (script_t *) hval;

        if(script_sem_take(script)) {
            script->fl_do_kill = SWITCH_TRUE;
            script_sem_release(script);
        }
    }

    switch_safe_free(hidx);
    switch_core_inthash_destroy(&globals.scripts_map);
    switch_mutex_unlock(globals.mutex_scripts);

    if(globals.qjs_rt) {
        JS_FreeRuntime(globals.qjs_rt);
    }
    return SWITCH_STATUS_SUCCESS;
}



