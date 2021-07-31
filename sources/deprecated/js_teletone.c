/**
 * Teletone
 * deprecated see: session.genTones
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define JS_BUFFER_SIZE  1024 * 32
#define JS_BLOCK_SIZE   JS_BUFFER_SIZE

#define CLASS_NAME     "TeleTone"
#define PROP_NAME      0

#define TONE_SANITY_CHECK() if (!js_teletone || !js_teletone->tgs_ptr) { \
           return JS_ThrowTypeError(ctx, "Teletone is not initialized"); \
        }

static JSClassID js_teletone_class_id;

static JSValue js_teletone_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static void js_teletone_finalizer(JSRuntime *rt, JSValue val);
static int teletone_handler(teletone_generation_session_t *tgs, teletone_tone_map_t *map);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_teletone_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_teletone_t *js_teletone = JS_GetOpaque2(ctx, this_val, js_teletone_class_id);

    if(!js_teletone) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_NAME: {
            return JS_NewString(ctx, js_teletone->timer_name);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_teletone_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_teletone_t *js_teletone = JS_GetOpaque2(ctx, this_val, js_teletone_class_id);

    return JS_FALSE;
}

static JSValue js_teletone_generate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_teletone_t *js_teletone = JS_GetOpaque2(ctx, this_val, js_teletone_class_id);
    switch_core_session_t *session = NULL;
    switch_channel_t *channel = NULL;
    switch_frame_t *read_frame = NULL;
    switch_frame_t write_frame = { 0 };
    char fdata[1024] = "";
    char dtmf[128] = "";
    const char *tone_data = NULL;
    uint32_t loops = 0, fl_success = 1;
    switch_status_t status;
    JSValue args[4] = { 0 };
    JSValue cb_ret;

    TONE_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    tone_data = JS_ToCString(ctx, argv[0]);
    if(zstr(tone_data)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: toneDate");
    }

    if(argc > 1) {
        JS_ToUint32(ctx, &loops, argv[1]);
    }

    if(!js_teletone->session) {
        return JS_FALSE;
    }

    session = js_teletone->session;
    channel = switch_core_session_get_channel(session);
    if(!switch_channel_ready(channel)) {
        return JS_FALSE;
    }

    if(js_teletone->audio_buffer) {
        switch_buffer_zero(js_teletone->audio_buffer);
    }

    js_teletone->tgs.debug = 1;
    js_teletone->tgs.debug_stream = switch_core_get_console();

    teletone_run(&js_teletone->tgs, tone_data);

    write_frame.codec = &js_teletone->codec;
    write_frame.data = fdata;
    write_frame.buflen = sizeof(fdata);

    if(js_teletone->timer_ptr) {
        switch_core_service_session(session);
    }

    if(loops) {
        switch_buffer_set_loops(js_teletone->audio_buffer, loops);
    }

    while(1) {
        if(!switch_channel_ready(channel)) {
            fl_success = 0;
            break;
        }

        if(js_teletone->fl_dtmf_hook) {
            if(switch_channel_has_dtmf(channel)) {
                switch_channel_dequeue_dtmf_string(channel, dtmf, sizeof(dtmf));

                args[0] = JS_NewString(ctx, dtmf);
                args[1] = js_teletone->cb_arg;

                cb_ret = JS_Call(ctx, js_teletone->dtmf_cb, this_val, 2, (JSValueConst *) args);
                if(JS_IsException(cb_ret)) {
                    ctx_dump_error(NULL, NULL, ctx);
                    JS_ResetUncatchableError(ctx);
                    JS_FreeValue(ctx, args[0]);
                    JS_FreeValue(ctx, cb_ret);
                    fl_success = 0;
                    break;
                }
                if(JS_IsBool(cb_ret) && JS_ToBool(ctx, cb_ret)) {
                    JS_FreeValue(ctx, args[0]);
                    JS_FreeValue(ctx, cb_ret);
                    break;
                }
                JS_FreeValue(ctx, args[0]);
                JS_FreeValue(ctx, cb_ret);
            }
        }

        if(js_teletone->timer_ptr) {
            if(switch_core_timer_next(js_teletone->timer_ptr) != SWITCH_STATUS_SUCCESS) {
                fl_success = 0; break;
            } else {
                status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
                if(!SWITCH_READ_ACCEPTABLE(status)) {
                    fl_success = 0; break;
                }
            }
        }

        write_frame.datalen = (uint32_t) switch_buffer_read_loop(js_teletone->audio_buffer, fdata, write_frame.codec->implementation->decoded_bytes_per_packet);
        if(write_frame.datalen <= 0) {
            break;
        }

        write_frame.samples = write_frame.datalen / 2;
        if(switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "write frame fail\n");
            fl_success = 0; break;
        }
    }

    if(js_teletone->timer_ptr) {
        switch_core_thread_session_end(session);
    }

    JS_FreeCString(ctx, tone_data);
    return fl_success ? JS_TRUE : JS_FALSE;
}

static JSValue js_teletone_tone_add(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_teletone_t *js_teletone = JS_GetOpaque2(ctx, this_val, js_teletone_class_id);

    TONE_SANITY_CHECK();

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    //
    // todo
    //

    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

static JSValue js_teletone_on_dtmf(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_teletone_t *js_teletone = JS_GetOpaque2(ctx, this_val, js_teletone_class_id);

    TONE_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    if(JS_IsFunction(ctx, argv[0])) {
        js_teletone->dtmf_cb = argv[0];
        js_teletone->cb_arg = (argc > 1 ? argv[1] : JS_UNDEFINED);
        js_teletone->fl_dtmf_hook = SWITCH_TRUE;
        return JS_TRUE;
    }
    if(JS_IsNull(argv[0]) || JS_IsUndefined(argv[0])) {
        js_teletone->fl_dtmf_hook = SWITCH_FALSE;
        return JS_TRUE;
    }

    return JS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// handlers
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static int teletone_handler(teletone_generation_session_t *tgs, teletone_tone_map_t *map) {
    js_teletone_t *js_teletone = tgs->user_data;
    int wrote;

    if(!js_teletone) {
        return -1;
    }
    wrote = teletone_mux_tones(tgs, map);
    switch_buffer_write(js_teletone->audio_buffer, tgs->buffer, wrote * 2);

    return 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_teletone_class = {
    CLASS_NAME,
    .finalizer = js_teletone_finalizer,
};

static const JSCFunctionListEntry js_teletone_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", js_teletone_property_get, js_teletone_property_set, PROP_NAME),
    //
    JS_CFUNC_DEF("generate", 1, js_teletone_generate),
    JS_CFUNC_DEF("addTone", 10, js_teletone_tone_add),
    JS_CFUNC_DEF("onDTMF", 1, js_teletone_on_dtmf),
};

static void js_teletone_finalizer(JSRuntime *rt, JSValue val) {
    js_teletone_t *js_teletone = JS_GetOpaque(val, js_teletone_class_id);

    if(!js_teletone) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-teletone-finalizer: js_teletone=%p\n", js_teletone);

    if(js_teletone->tgs_ptr) {
        teletone_destroy_session(js_teletone->tgs_ptr);
    }
    if(js_teletone->timer_ptr) {
        switch_core_timer_destroy(js_teletone->timer_ptr);
    }
    if(js_teletone->codec_ptr) {
        switch_core_codec_destroy(js_teletone->codec_ptr);
    }
    if(js_teletone->audio_buffer) {
        switch_buffer_destroy(&js_teletone->audio_buffer);
    }
    if(js_teletone->pool) {
        switch_core_destroy_memory_pool(&js_teletone->pool);
    }

    js_free_rt(rt, js_teletone);
}

static JSValue js_teletone_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_teletone_t *js_teletone = NULL;
    switch_memory_pool_t *pool = NULL;
    js_session_t *jss = NULL;
    switch_codec_implementation_t read_impl = { 0 };
    switch_status_t status;
    const char *timer_name = NULL;

    js_teletone = js_mallocz(ctx, sizeof(js_teletone_t));
    if(!js_teletone) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }
    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    jss = JS_GetOpaque(argv[0], js_seesion_class_get_id());
    if(!jss || !jss->session) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: session");
        goto fail;
    }

    switch_core_session_get_read_impl(jss->session, &read_impl);

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    if(argc > 1) {
        timer_name = JS_ToCString(ctx, argv[1]);
        if(!zstr(timer_name)) {
            uint32_t ms = read_impl.microseconds_per_packet / 1000;
            js_teletone->timer_name = switch_core_strdup(pool, timer_name);

            if(switch_core_timer_init(&js_teletone->timer, js_teletone->timer_name, ms, (read_impl.samples_per_second / 50) * read_impl.number_of_channels, pool) != SWITCH_STATUS_SUCCESS) {
                err = JS_ThrowTypeError(ctx, "Couldn't init timer");
                goto fail;
            }
            js_teletone->timer_ptr = &js_teletone->timer;
        }

        JS_FreeCString(ctx, timer_name);
    }

    status = switch_core_codec_init(&js_teletone->codec, "L16", NULL, NULL, read_impl.actual_samples_per_second, read_impl.microseconds_per_packet / 1000, read_impl.number_of_channels, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL, pool);
    if(status != SWITCH_STATUS_SUCCESS) {
        err = JS_ThrowTypeError(ctx, "Couldn't activate codec");
        goto fail;
    }

    switch_buffer_create_dynamic(&js_teletone->audio_buffer, JS_BLOCK_SIZE, JS_BUFFER_SIZE, 0);
    teletone_init_session(&js_teletone->tgs, 0, teletone_handler, js_teletone);

    js_teletone->pool = pool;
    js_teletone->session = jss->session;
    js_teletone->tgs_ptr = &js_teletone->tgs;
    js_teletone->codec_ptr = &js_teletone->codec;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_teletone_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_teletone);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-teletone-constructor: js_teletone=%p\n", js_teletone);

    return obj;

fail:
    JS_FreeCString(ctx, timer_name);
    if(js_teletone) {
        if(js_teletone->audio_buffer) {switch_buffer_destroy(&js_teletone->audio_buffer);}
        if(js_teletone->timer_ptr) { switch_core_timer_destroy(js_teletone->timer_ptr); }
        if(js_teletone->codec_ptr) { switch_core_codec_destroy(js_teletone->codec_ptr); }
        if(js_teletone->tgs_ptr) {teletone_destroy_session(js_teletone->tgs_ptr);}
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_teletone) {
        js_free(ctx, js_teletone);
    }
    JS_FreeValue(ctx, obj);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_teletone_class_get_id() {
    return js_teletone_class_id;
}

switch_status_t js_teletone_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_teletone_class_id) {
        JS_NewClassID(&js_teletone_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_teletone_class_id, &js_teletone_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_teletone_proto_funcs, ARRAY_SIZE(js_teletone_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_teletone_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_teletone_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}
