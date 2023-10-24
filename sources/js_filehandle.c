/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include "js_filehandle.h"

#define CLASS_NAME          "FileHandle"
#define FH_PROP_SPEED       0
#define FH_PROP_VOLUME      1
#define FH_PROP_CHANNELS    2
#define FH_PROP_SAMPLERATE  3

#define FH_SANITY_CHECK() do { \
        if(!js_fh || !js_fh->fh || js_fh->fl_closed) { \
           return JS_ThrowTypeError(ctx, "Handle is not initialized"); \
        } \
        if(js_fh->fl_closed) { \
           return JS_ThrowTypeError(ctx, "Handle is closed"); \
        } \
    } while(0)

#define FH_SESSION_CHECK() if (!js_fh->session) { \
           return JS_ThrowTypeError(ctx, "Session is not initialized"); \
        }

static void js_fh_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_fh_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));

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
        case FH_PROP_CHANNELS: {
            return JS_NewInt32(ctx, js_fh->fh->channels);
        }
        case FH_PROP_SAMPLERATE: {
            return JS_NewInt32(ctx, js_fh->fh->samplerate);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_fh_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));
    const char *sval = NULL;
    int32_t ival = 0;

    if(!js_fh || !js_fh->fh) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case FH_PROP_CHANNELS:
        case FH_PROP_SAMPLERATE:
            return JS_FALSE;
        break;

        case FH_PROP_SPEED: {
            if(JS_IsNumber(val)) {
                JS_ToUint32(ctx, &ival, val);
                js_fh->fh->speed = ival;
            } else {
                if(QJS_IS_NULL(val)) {
                    js_fh->fh->speed = 0;
                } else {
                    sval = JS_ToCString(ctx, val);
                    if(sval[0] == '+' || sval[0] == '-') {
                        int step = atoi(sval);
                        js_fh->fh->speed += (!step ? 1 : step);
                    } else {
                        js_fh->fh->speed = atoi(sval);
                    }
                    JS_FreeCString(ctx, sval);
                }
            }
            return JS_TRUE;
        }
        case FH_PROP_VOLUME: {
            if(JS_IsNumber(val)) {
                JS_ToUint32(ctx, &ival, val);
                js_fh->fh->vol = ival;
            } else {
                if(QJS_IS_NULL(val)) {
                    js_fh->fh->vol = 0;
                } else {
                    sval = JS_ToCString(ctx, val);
                    if(zstr(sval)) {
                        js_fh->fh->vol = 0;
                    } else {
                        if(sval[0] == '+' || sval[0] == '-') {
                            int step = atoi(sval);
                            js_fh->fh->vol += (!step ? 1 : step);
                        } else {
                            js_fh->fh->vol = atoi(sval);
                        }
                    }
                    JS_FreeCString(ctx, sval);
                }
            }
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSValue js_fh_pause(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));

    FH_SANITY_CHECK();

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
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));

    FH_SANITY_CHECK();

    switch_core_file_truncate(js_fh->fh, 0);

    return JS_FALSE;
}

static JSValue js_fh_restart(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));
    uint32_t pos = 0;

    FH_SANITY_CHECK();

    js_fh->fh->speed = 0;
    switch_core_file_seek(js_fh->fh, &pos, 0, SEEK_SET);

    return JS_FALSE;
}

static JSValue js_fh_seek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));
    switch_codec_t *codec = NULL;
    const char *val = NULL;
    uint32_t samps = 0;
    uint32_t pos = 0;
    int step = 0;

    FH_SANITY_CHECK();
    FH_SESSION_CHECK();

    codec = switch_core_session_get_read_codec(js_fh->session);

    if(QJS_IS_NULL(argv[0])) {
        return JS_FALSE;
    }

    val = JS_ToCString(ctx, argv[0]);

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
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    FH_SANITY_CHECK();

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
        return JS_ThrowRangeError(ctx, "Buffer overflow (len > array size)");
    }

    if(switch_core_file_read(js_fh->fh, buf, &len) != SWITCH_STATUS_SUCCESS) {
        if(len == 0) {
            return JS_NewInt64(ctx, 0);
        }
        return JS_EXCEPTION;
    }

    return JS_NewInt64(ctx, len);
}

static JSValue js_fh_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    FH_SANITY_CHECK();

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
        return JS_ThrowRangeError(ctx, "Buffer overflow (len > array size)");
    }

    if(switch_core_file_write(js_fh->fh, buf, &len) != SWITCH_STATUS_SUCCESS) {
        if(len == 0) {
            return JS_NewInt64(ctx, 0);
        }
        return JS_EXCEPTION;
    }

    return JS_NewInt64(ctx, len);
}

static JSValue js_fh_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));

    FH_SANITY_CHECK();

    if(js_fh->fh) {
        switch_core_file_close(js_fh->fh);
    }
    js_fh->fl_closed = SWITCH_TRUE;

    return JS_TRUE;
}

static JSValue js_fh_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_handle_t *js_fh = JS_GetOpaque2(ctx, this_val, js_file_handle_get_classid(ctx));

    FH_SANITY_CHECK();

    if(js_fh->fh) {
        switch_set_flag(js_fh->fh, SWITCH_FILE_DONE);
    }

    return JS_TRUE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_fh_class = {
    CLASS_NAME,
    .finalizer = js_fh_finalizer,
};

static const JSCFunctionListEntry js_fh_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("speed", js_fh_property_get, js_fh_property_set, FH_PROP_SPEED),
    JS_CGETSET_MAGIC_DEF("volume", js_fh_property_get, js_fh_property_set, FH_PROP_VOLUME),
    JS_CGETSET_MAGIC_DEF("channels", js_fh_property_get, js_fh_property_set, FH_PROP_CHANNELS),
    JS_CGETSET_MAGIC_DEF("samplerate", js_fh_property_get, js_fh_property_set, FH_PROP_SAMPLERATE),
    //
    JS_CFUNC_DEF("pause", 1, js_fh_pause),
    JS_CFUNC_DEF("truncate", 1, js_fh_truncate),
    JS_CFUNC_DEF("restart", 1, js_fh_restart),
    JS_CFUNC_DEF("seek", 1, js_fh_seek),
    JS_CFUNC_DEF("read", 1, js_fh_read),
    JS_CFUNC_DEF("write", 1, js_fh_write),
    JS_CFUNC_DEF("close", 1, js_fh_close),
    JS_CFUNC_DEF("stop", 1, js_fh_stop),
};

static void js_fh_finalizer(JSRuntime *rt, JSValue val) {
    js_file_handle_t *js_fh = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));

    if(!js_fh) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-fh-finalizer: js_fh=%p, fh=%p\n", js_fh, js_fh->fh);

    if(js_fh->fh && js_fh->fl_auto_close && !js_fh->fl_closed) {
        switch_core_file_close(js_fh->fh);
    }

    js_free_rt(rt, js_fh);
}

static JSValue js_fh_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_file_handle_t *js_fh = NULL;
    switch_file_handle_t *fh = NULL;
    switch_channel_t *channel = NULL;
    switch_codec_t *codec = NULL;
    js_session_t *jss = NULL;
    uint32_t flags = 0x0;
    const char *path = NULL;
    char *dpath = NULL;

    if(argc > 0) {
        if(QJS_IS_NULL(argv[0])) {
            return JS_ThrowTypeError(ctx, "Invalid argument: filename");
        }
        path = JS_ToCString(ctx, argv[0]);

        if(argc > 1) {
            jss = JS_GetOpaque(argv[1], js_seesion_get_classid(ctx));
            if(!jss || !jss->session) {
                err = JS_ThrowTypeError(ctx, "invalid argument: session");
                goto fail;
            }
            channel = switch_core_session_get_channel(jss->session);
            if(!switch_channel_ready(channel)) {
                err = JS_ThrowTypeError(ctx, "Channel is not ready");
                goto fail;
            }
            codec = switch_core_session_get_read_codec(js_fh->session);
        }

        if(switch_file_exists(path, NULL) != SWITCH_STATUS_SUCCESS) {
            const char *snd_prefix = switch_core_get_variable("sound_prefix");
            dpath = switch_mprintf("%s%s%s", snd_prefix, SWITCH_PATH_SEPARATOR, path);
            if(switch_file_exists(dpath, NULL) != SWITCH_STATUS_SUCCESS) {
                err = JS_ThrowTypeError(ctx, "File not found: %s", dpath);
                goto fail;
            }
        }
        if(argc > 2) {
            if(!QJS_IS_NULL(argv[2])) {
                const char *flstr = JS_ToCString(ctx, argv[2]);
                if(strstr(flstr, "read")) {
                    flags |= SWITCH_FILE_FLAG_READ;
                }
                if(strstr(flstr, "write")) {
                    flags |= SWITCH_FILE_FLAG_WRITE;
                }
                if(strstr(flstr, "short")) {
                    flags |= SWITCH_FILE_DATA_SHORT;
                }
                if(strstr(flstr, "int")) {
                    flags |= SWITCH_FILE_DATA_INT;
                }
                if(strstr(flstr, "float")) {
                    flags |= SWITCH_FILE_DATA_FLOAT;
                }
                if(strstr(flstr, "double")) {
                    flags |= SWITCH_FILE_DATA_DOUBLE;
                }
                if(strstr(flstr, "raw")) {
                    flags |= SWITCH_FILE_DATA_RAW;
                }
                JS_FreeCString(ctx, flstr);
            }
        }
    }
    if(jss && codec && (dpath || path)) {
        if(switch_core_file_open(fh, (dpath ? dpath : path), codec->implementation->number_of_channels, codec->implementation->samples_per_second, flags, NULL) != SWITCH_STATUS_SUCCESS) {
            err = JS_ThrowTypeError(ctx, "Couldn't open file: %s", (dpath ? dpath : path));
            goto fail;
        }
    }

    js_fh = js_mallocz(ctx, sizeof(js_file_handle_t));
    if(!js_fh) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_file_handle_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    js_fh->fh = fh;
    js_fh->session = (jss ? jss->session : NULL);
    js_fh->fl_auto_close = SWITCH_TRUE;
    JS_SetOpaque(obj, js_fh);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-fh-constructor: js_fh=%p, fh=%p\n", js_fh, js_fh->fh);

    switch_safe_free(dpath);
    JS_FreeCString(ctx, path);
    return obj;

fail:
    switch_safe_free(dpath);
    JS_FreeCString(ctx, path);
    if(fh) {
        switch_core_file_close(js_fh->fh);
    }
    if(js_fh) {
        js_free(ctx, js_fh);
    }
    JS_FreeValue(ctx, obj);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_file_handle_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_file_handle_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_file_handle_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_fh_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_fh_proto_funcs, ARRAY_SIZE(js_fh_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_fh_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

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
    JS_SetPropertyFunctionList(ctx, proto, js_fh_proto_funcs, ARRAY_SIZE(js_fh_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_file_handle_get_classid(ctx));
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_fh->fh = fh;
    js_fh->session = session;
    JS_SetOpaque(obj, js_fh);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-fh-obj-created: js_fh=%p, fh=%p\n", js_fh, js_fh->fh);

    return obj;
}
