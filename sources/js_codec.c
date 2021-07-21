/**
 * Codecs
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME          "Codec"
#define PROP_NAME           0
#define PROP_PTIME          1
#define PROP_CHANNELS       2
#define PROP_SAMPLERATE     3
#define PROP_CAN_ENCODE     4
#define PROP_CAN_DECODE     5

#define CODEC_SANITY_CHECK() if (!js_codec || !js_codec->codec) { \
           return JS_ThrowTypeError(ctx, "Codec is not initialized"); \
        }

static JSClassID js_codec_class_id;

static void js_codec_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_codec_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_codec_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_codec_t *js_codec = JS_GetOpaque2(ctx, this_val, js_codec_class_id);

    if(!js_codec) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_NAME:
            return JS_NewString(ctx, js_codec->name);

        case PROP_PTIME:
            return JS_NewInt32(ctx, js_codec->ptime);

        case PROP_CHANNELS:
            return JS_NewInt32(ctx, js_codec->channels);

        case PROP_SAMPLERATE:
            return JS_NewInt32(ctx, js_codec->samplerate);

        case PROP_CAN_ENCODE:
            return (js_codec->flags & SWITCH_CODEC_FLAG_ENCODE) ? JS_TRUE : JS_FALSE;

        case PROP_CAN_DECODE:
            return (js_codec->flags & SWITCH_CODEC_FLAG_DECODE) ? JS_TRUE : JS_FALSE;
    }

    return JS_UNDEFINED;
}

static JSValue js_codec_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_codec_t *js_codec = JS_GetOpaque2(ctx, this_val, js_codec_class_id);

    return JS_FALSE;
}

static JSValue js_codec_ready(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_codec_t *js_codec = JS_GetOpaque2(ctx, this_val, js_codec_class_id);

    if(!js_codec || !js_codec->codec) {
        return JS_FALSE;
    }

    return (switch_core_codec_ready(js_codec->codec) ? JS_TRUE : JS_FALSE);
}

static JSValue js_codec_encode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_codec_t *js_codec = JS_GetOpaque2(ctx, this_val, js_codec_class_id);
    switch_status_t status;
    switch_size_t src_buf_size = 0, dst_buf_size = 0;
    uint32_t src_samplerate = 0, src_len = 0, dst_samplerate = 0, dst_len = 0, flags = 0;
    char *src_buf = NULL;
    char *dst_buf = NULL;

    CODEC_SANITY_CHECK();

    if(argc < 3) {
       return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    if(!switch_core_codec_ready(js_codec->codec)) {
        return JS_ThrowTypeError(ctx, "Codec is not ready");
    }

    if(!(js_codec->flags & SWITCH_CODEC_FLAG_ENCODE)) {
        return JS_ThrowTypeError(ctx, "Encode is not supported");
    }

    src_buf = JS_GetArrayBuffer(ctx, &src_buf_size, argv[0]);
    if(!src_buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcBuffer");
    }

    dst_buf = JS_GetArrayBuffer(ctx, &dst_buf_size, argv[3]);
    if(!dst_buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: dstBuffer");
    }

    JS_ToInt32(ctx, &src_samplerate, argv[1]);
    JS_ToInt32(ctx, &src_len, argv[2]);

    if(!src_samplerate) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcSamplerate");
    }
    if(!src_len) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcLength");
    }
    if(src_len > src_buf_size) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcLength > srcBufferSize");
    }

    dst_len = dst_buf_size;
    dst_samplerate = src_samplerate;
    status = switch_core_codec_encode(js_codec->codec, NULL, src_buf, src_len, src_samplerate, dst_buf, &dst_len, &dst_samplerate, &flags);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't encode buffer (err:%d)\n", status);
        return JS_NewInt32(ctx, 0);
    }

    return JS_NewInt32(ctx, dst_len);
}

static JSValue js_codec_decode(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_codec_t *js_codec = JS_GetOpaque2(ctx, this_val, js_codec_class_id);
    switch_status_t status;
    switch_size_t src_buf_size = 0, dst_buf_size = 0;
    uint32_t src_samplerate = 0, src_len = 0, dst_samplerate = 0, dst_len = 0, flags = 0;
    char *src_buf = NULL;
    char *dst_buf = NULL;

    CODEC_SANITY_CHECK();

    if(argc < 3) {
       return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    if(!switch_core_codec_ready(js_codec->codec)) {
        return JS_ThrowTypeError(ctx, "Codec is not ready");
    }

    if(!(js_codec->flags & SWITCH_CODEC_FLAG_DECODE)) {
        return JS_ThrowTypeError(ctx, "Decode is not supported");
    }

    src_buf = JS_GetArrayBuffer(ctx, &src_buf_size, argv[0]);
    if(!src_buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcBuffer");
    }

    dst_buf = JS_GetArrayBuffer(ctx, &dst_buf_size, argv[3]);
    if(!dst_buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: dstBuffer");
    }

    JS_ToInt32(ctx, &src_samplerate, argv[1]);
    JS_ToInt32(ctx, &src_len, argv[2]);

    if(!src_samplerate) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcSamplerate");
    }
    if(!src_len) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcLength");
    }
    if(src_len > src_buf_size) {
        return JS_ThrowTypeError(ctx, "Invalid argument: srcLength > srcBufferSize");
    }

    dst_len = dst_buf_size;
    dst_samplerate = src_samplerate;
    status = switch_core_codec_decode(js_codec->codec, NULL, src_buf, src_len, src_samplerate, dst_buf, &dst_len, &dst_samplerate, &flags);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't encode buffer (err:%d)\n", status);
        return JS_NewInt32(ctx, 0);
    }

    return JS_NewInt32(ctx, dst_len);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_codec_class = {
    CLASS_NAME,
    .finalizer = js_codec_finalizer,
};

static const JSCFunctionListEntry js_codec_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", js_codec_property_get, js_codec_property_set, PROP_NAME),
    JS_CGETSET_MAGIC_DEF("ptime", js_codec_property_get, js_codec_property_set, PROP_PTIME),
    JS_CGETSET_MAGIC_DEF("channels", js_codec_property_get, js_codec_property_set, PROP_CHANNELS),
    JS_CGETSET_MAGIC_DEF("samplerate", js_codec_property_get, js_codec_property_set, PROP_SAMPLERATE),
    JS_CGETSET_MAGIC_DEF("canEncode", js_codec_property_get, js_codec_property_set, PROP_CAN_ENCODE),
    JS_CGETSET_MAGIC_DEF("canDecode", js_codec_property_get, js_codec_property_set, PROP_CAN_DECODE),
    //
    JS_CFUNC_DEF("ready", 0, js_codec_ready),
    JS_CFUNC_DEF("encode", 4, js_codec_encode),
    JS_CFUNC_DEF("decode", 4, js_codec_decode),
};

static void js_codec_finalizer(JSRuntime *rt, JSValue val) {
    js_codec_t *js_codec = JS_GetOpaque(val, js_codec_class_id);

    if(!js_codec) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-codec-finalizer: js_codec=%p, codec=%p\n", js_codec, js_codec->codec);

    if(js_codec->codec && js_codec->fl_can_destroy) {
        switch_core_codec_destroy(js_codec->codec);
    }

    if(js_codec->pool) {
        switch_core_destroy_memory_pool(&js_codec->pool);
    }

    js_free_rt(rt, js_codec);
}

static JSValue js_codec_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    switch_status_t status;
    js_session_t *jss = NULL;
    js_codec_t *js_codec = NULL;
    switch_memory_pool_t *pool = NULL;
    uint32_t samplerate = 0, channels = 0, ptime = 0;
    const char *name = NULL;

    if(argc < 4) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    jss = JS_GetOpaque(argv[0], js_seesion_class_get_id());
    if(!jss || !jss->session) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: session");
        goto fail;
    }

    JS_ToInt32(ctx, &samplerate, argv[2]);
    if(samplerate <= 0) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: samplerate");
        goto fail;
    }

    JS_ToInt32(ctx, &channels, argv[3]);
    if(channels <= 0) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: channels");
        goto fail;
    }

    JS_ToInt32(ctx, &ptime, argv[4]);
    if(ptime <= 0) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: ptime");
        goto fail;
    }

    name = JS_ToCString(ctx, argv[1]);
    if(zstr(name)) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: codecName");
        goto fail;
    }

    js_codec = js_mallocz(ctx, sizeof(js_codec_t));
    if(!js_codec) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    js_codec->pool = pool;
    js_codec->name = switch_core_strdup(pool, name);
    js_codec->samplerate = samplerate;
    js_codec->channels = channels;
    js_codec->ptime = ptime;
    js_codec->flags = SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE;
    js_codec->fl_can_destroy = SWITCH_TRUE;

    status = switch_core_codec_init(&js_codec->codec_base, js_codec->name, NULL, NULL, samplerate, ptime, channels, js_codec->flags, NULL, pool);
    if(status != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Couldn't activate codec (err: %d)\n", status);
        err = JS_ThrowTypeError(ctx, "Codec activation filure");
        goto fail;
    }
    js_codec->codec = &js_codec->codec_base;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_codec_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_codec);
    JS_FreeCString(ctx, name);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-codec-constructor: js_codec=%p, codec=%p\n", js_codec, js_codec->codec);

    return obj;

fail:
    if(js_codec) {
        if(js_codec->codec) {
            switch_core_codec_destroy(js_codec->codec);
        }
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_codec) {
        js_free(ctx, js_codec);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, name);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_codec_class_get_id() {
    return js_codec_class_id;
}

switch_status_t js_codec_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_codec_class_id) {
        JS_NewClassID(&js_codec_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_codec_class_id, &js_codec_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_codec_proto_funcs, ARRAY_SIZE(js_codec_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_codec_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_codec_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}


JSValue js_codec_from_session_wcodec(JSContext *ctx, switch_core_session_t *session) {
    js_codec_t *js_codec = NULL;
    JSValue obj, proto;
    switch_channel_t *channel = NULL;
    switch_codec_t *codec = NULL;
    const char *name = NULL;

    js_codec = js_mallocz(ctx, sizeof(js_codec_t));
    if(!js_codec) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_codec_proto_funcs, ARRAY_SIZE(js_codec_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_codec_class_id);
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    channel = switch_core_session_get_channel(session);

    codec = switch_core_session_get_write_codec(session);
    if(!codec) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Channel has no media\n");
        return JS_EXCEPTION;
    }

    name = switch_channel_get_variable(channel, "write_codec");

    js_codec->name = name;
    js_codec->codec = codec;
    js_codec->samplerate = codec->implementation->samples_per_second;
    js_codec->channels = codec->implementation->number_of_channels;
    js_codec->ptime = codec->implementation->microseconds_per_packet / 1000;
    js_codec->flags = SWITCH_CODEC_FLAG_ENCODE;
    js_codec->fl_can_destroy = SWITCH_FALSE;

    JS_SetOpaque(obj, js_codec);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-wcodec-from-session: js_codec=%p, codec=%p\n", js_codec, js_codec->codec);

    return obj;
}

JSValue js_codec_from_session_rcodec(JSContext *ctx, switch_core_session_t *session) {
    js_codec_t *js_codec = NULL;
    JSValue obj, proto;
    switch_channel_t *channel = NULL;
    switch_codec_t *codec = NULL;
    const char *name = NULL;

    js_codec = js_mallocz(ctx, sizeof(js_codec_t));
    if(!js_codec) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_codec_proto_funcs, ARRAY_SIZE(js_codec_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_codec_class_id);
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    channel = switch_core_session_get_channel(session);

    codec = switch_core_session_get_read_codec(session);
    if(!codec) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Channel has no media\n");
        return JS_EXCEPTION;
    }

    name = switch_channel_get_variable(channel, "read_codec");

    js_codec->name = name;
    js_codec->codec = codec;
    js_codec->samplerate = codec->implementation->samples_per_second;
    js_codec->channels = codec->implementation->number_of_channels;
    js_codec->ptime = codec->implementation->microseconds_per_packet / 1000;
    js_codec->flags = SWITCH_CODEC_FLAG_DECODE;
    js_codec->fl_can_destroy = SWITCH_FALSE;

    JS_SetOpaque(obj, js_codec);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-rcodec-from-session: js_codec=%p, codec=%p\n", js_codec, js_codec->codec);

    return obj;
}
