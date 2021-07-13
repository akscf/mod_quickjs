/**
 * Freeswitch file
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define FILE_HANDLE_CLASS_NAME      "FileHandle"
#define FH_PROP_SPEED               0
#define FH_PROP_VOLUME              1

#define FH_SANITY_CHECK() if (!js_fh || !js_fh->fh) { \
           return JS_ThrowTypeError(ctx, "Handle is not initialized"); \
        }

#define FH_SESSION_CHECK() if (!js_fh->session) { \
           return JS_ThrowTypeError(ctx, "Session no active"); \
        }

#define FH_CLOSE_CHECK() if (js_fh->fl_closed) { \
           return JS_ThrowTypeError(ctx, "File alrady closed"); \
        }

static JSClassID js_fh_class_id;
static void js_file_handle_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_fh_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    if(!js_fh || !js_fh->fh) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case FH_PROP_SPEED: {
            return JS_NewInt32(ctx, js_fh->fh->speed);
        }
        case FH_PROP_VOLUME: {
            return JS_NewInt32(ctx, js_fh->fh->vol);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_fh_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);
    const char *sval = NULL;
    int32_t ival = 0;

    if(!js_fh || !js_fh->fh) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case FH_PROP_SPEED: {
            if(JS_IsNumber(val)) {
                JS_ToUint32(ctx, &ival, val);
                js_fh->fh->speed = ival;
            } else {
                sval = JS_ToCString(ctx, val);
                if(zstr(sval)) {
                    js_fh->fh->speed = 0;
                } else {
                    if(sval[0] == '+' || sval[0] == '-') {
                        int step = atoi(sval + 1);
                        if(!step) { step = 1;}
                        js_fh->fh->speed += step;
                    } else {
                        js_fh->fh->speed = atoi(sval);
                    }
                }
            }
            JS_FreeCString(ctx, sval);
            return JS_TRUE;
        }
        case FH_PROP_VOLUME: {
            if(JS_IsNumber(val)) {
                JS_ToUint32(ctx, &ival, val);
                js_fh->fh->vol = ival;
            } else {
                sval = JS_ToCString(ctx, val);
                if(zstr(sval)) {
                    js_fh->fh->vol = 0;
                } else {
                    if(sval[0] == '+' || sval[0] == '-') {
                        int step = atoi(sval + 1);
                        if(!step) { step = 1;}
                        js_fh->fh->vol += step;
                    } else {
                        js_fh->fh->vol = atoi(sval);
                    }
                }
            }
            JS_FreeCString(ctx, sval);
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSValue js_fh_pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    if(argc > 0) {
        if(JS_ToBool(ctx, argv[0])) {
            switch_set_flag(js_fh->fh, SWITCH_FILE_PAUSE);
        } else {
            switch_clear_flag(js_fh->fh, SWITCH_FILE_PAUSE);
        }
    } else {
        if(switch_test_flag(js_fh->fh, SWITCH_FILE_PAUSE)) {
            switch_clear_flag(js_fh->fh, SWITCH_FILE_PAUSE);
        } else {
            switch_set_flag(js_fh->fh, SWITCH_FILE_PAUSE);
        }
    }

    return JS_FALSE;
}

static JSValue js_fh_truncate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    switch_core_file_truncate(js_fh->fh, 0);

    return JS_FALSE;
}

static JSValue js_fh_restart(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);
    uint32_t pos = 0;

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    js_fh->fh->speed = 0;
    switch_core_file_seek(js_fh->fh, &pos, 0, SEEK_SET);

    return JS_FALSE;
}

static JSValue js_fh_seek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);
    switch_codec_t *codec = NULL;
    const char *val = NULL;
    uint32_t samps = 0;
    uint32_t pos = 0;
    int step = 0;

    FH_SANITY_CHECK();
    FH_SESSION_CHECK();
    FH_CLOSE_CHECK();

    codec = switch_core_session_get_read_codec(js_fh->session);

    val = JS_ToCString(ctx, argv[0]);
    if(zstr(val)) {
        return JS_FALSE;
    }

    if(val[0] == '+' || val[0] == '-') {
        step = atoi(val + 1);
        if(!step) { step = 1000;}
        if(step > 0) {
            samps = step * (codec->implementation->actual_samples_per_second / 1000);
            switch_core_file_seek(js_fh->fh, &pos, samps, SEEK_CUR);
        } else {
            samps = abs(step) * (codec->implementation->actual_samples_per_second / 1000);
            switch_core_file_seek(js_fh->fh, &pos, (js_fh->fh->pos - samps), SEEK_SET);
        }
    } else {
        samps = atoi(val) * (codec->implementation->actual_samples_per_second / 1000);
        switch_core_file_seek(js_fh->fh, &pos, samps, SEEK_SET);
    }

    JS_FreeCString(ctx, val);

    return JS_TRUE;
}

static JSValue js_fh_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_fh_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    //
    // todo
    //

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_fh_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_fh_class_id);

    FH_SANITY_CHECK();
    FH_CLOSE_CHECK();

    switch_core_file_close(js_fh->fh);
    js_fh->fl_closed = SWITCH_TRUE;

    return JS_TRUE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_file_handle_class = {
    FILE_HANDLE_CLASS_NAME,
    .finalizer = js_file_handle_finalizer,
};

static const JSCFunctionListEntry js_file_handle_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("speed", js_fh_property_get, js_fh_property_set, FH_PROP_SPEED),
    JS_CGETSET_MAGIC_DEF("volume", js_fh_property_get, js_fh_property_set, FH_PROP_VOLUME),
    //
    JS_CFUNC_DEF("pause", 1, js_fh_pause),
    JS_CFUNC_DEF("truncate", 1, js_fh_truncate),
    JS_CFUNC_DEF("restart", 1, js_fh_restart),
    JS_CFUNC_DEF("seek", 1, js_fh_seek),
    JS_CFUNC_DEF("read", 1, js_fh_read),
    JS_CFUNC_DEF("write", 1, js_fh_write),
    JS_CFUNC_DEF("close", 1, js_fh_close),
};

static void js_file_handle_finalizer(JSRuntime *rt, JSValue val) {
    js_file_handle_t *js_fh = JS_GetOpaque(val, js_fh_class_id);

    if(!js_fh) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-fh-finalizer: js_file_handle=%p, fh=%p\n", js_fh, js_fh->fh);

    if(js_fh->fh && js_fh->fl_auto_close && !js_fh->fl_closed) {
        switch_core_file_close(js_fh->fh);
    }

    js_free_rt(rt, js_fh);
}

static JSValue js_file_handle_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    js_file_handle_t *js_fh = NULL;
    switch_file_handle_t *fh = NULL;
    const char *fname = NULL;

    js_fh = js_mallocz(ctx, sizeof(js_file_handle_t));
    if(!js_fh) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    if(argc > 0) {
        fname = JS_ToCString(ctx, argv[0]);
    }
    if(zstr(fname)) {
        JS_ThrowTypeError(ctx, "Missing filename");
    }
    //
    // todo
    //
    JS_FreeCString(ctx, fname);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_fh_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    js_fh->fh = fh;
    js_fh->fl_auto_close = SWITCH_TRUE;
    JS_SetOpaque(obj, js_fh);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-fh-constructor: js-filehandle=%p, fh=%p\n", js_fh, js_fh->fh);

    return obj;
fail:
    if(fh) {
        switch_core_file_close(js_fh->fh);
    }
    js_free(ctx, js_fh);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_file_handle_class_get_id() {
    return js_fh_class_id;
}

void js_file_handle_class_register_rt(JSRuntime *rt) {
    JS_NewClassID(&js_fh_class_id);
    JS_NewClass(rt, js_fh_class_id, &js_file_handle_class);
}

switch_status_t js_file_handle_class_register_ctx(JSContext *ctx, JSValue global_obj) {
    JSValue fh_proto;
    JSValue fh_class;

    fh_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, fh_proto, js_file_handle_proto_funcs, ARRAY_SIZE(js_file_handle_proto_funcs));

    fh_class = JS_NewCFunction2(ctx, js_file_handle_contructor, FILE_HANDLE_CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, fh_class, fh_proto);
    JS_SetClassProto(ctx, js_fh_class_id, fh_proto);

    JS_SetPropertyStr(ctx, global_obj, FILE_HANDLE_CLASS_NAME, fh_class);

    return SWITCH_STATUS_SUCCESS;
}

JSValue js_file_handle_object_create(JSContext *ctx, switch_file_handle_t *fh, switch_core_session_t *session) {
    js_file_handle_t *js_fh;
    JSValue obj, proto;

    js_fh = js_mallocz(ctx, sizeof(js_file_handle_t));
    if(!js_fh) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_file_handle_proto_funcs, ARRAY_SIZE(js_file_handle_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_fh_class_id);
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_fh->fh = fh;
    js_fh->session = session;
    JS_SetOpaque(obj, js_fh);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-fh-obj-created: js_file_handle=%p, fh=%p\n", js_fh, js_fh->fh);

    return obj;
}
