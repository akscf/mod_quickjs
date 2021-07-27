/**
 * EventHandler
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME      "EventHandler"
#define PROP_READY      1

#define MAX_QUEUE_LEN   100000

#define EH_SANITY_CHECK() if (!js_eventhandler || !js_eventhandler->event_queue) { \
           return JS_ThrowTypeError(ctx, "Handler is not initialized"); \
        }

static JSClassID js_eventhandler_class_id;
static void js_eventhandler_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_eventhandler_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);

    if(!js_eventhandler) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_READY: {
            return (js_eventhandler->event_queue ? JS_TRUE : JS_FALSE);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_eventhandler_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    return JS_FALSE;
}

static JSValue js_eventhandler_subscribe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    switch_event_types_t etype;
    int i = 0, j = 0, custom = 0, success = 0;

    EH_SANITY_CHECK();

    for(i = 0; i < argc; i++) {
        const char *ename = JS_ToCString(ctx, argv[0]);

        if(zstr(ename)) {
            continue;
        }
        if(custom) {
            char *t = switch_core_strdup(js_eventhandler->pool, ename);

            switch_mutex_lock(js_eventhandler->mutex);
            switch_core_hash_insert(js_eventhandler->custom_events, t, "1");
            switch_mutex_unlock(js_eventhandler->mutex);

            JS_FreeCString(ctx, ename);
            continue;
        }
        if(switch_name_event(ename, &etype) == SWITCH_STATUS_SUCCESS) {
            if(etype == SWITCH_EVENT_CUSTOM) {
                custom = 1;
            }
            if(etype == SWITCH_EVENT_ALL) {
                for(j = 0; j < SWITCH_EVENT_ALL; j++) {
                    js_eventhandler->event_list[j] = SWITCH_TRUE;
                }
            } else {
                js_eventhandler->event_list[etype] = SWITCH_TRUE;
            }
            success++;
        }
        JS_FreeCString(ctx, ename);
    }

    return (success ? JS_TRUE : JS_FALSE);
}

static JSValue js_eventhandler_unsubscribe(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    switch_event_types_t etype;
    int i = 0, j = 0, custom = 0, success = 0;

    EH_SANITY_CHECK();

    for(i = 0; i < argc; i++) {
        const char *ename = JS_ToCString(ctx, argv[0]);

        if(zstr(ename)) {
            continue;
        }
        if(custom) {
            switch_mutex_lock(js_eventhandler->mutex);
            switch_core_hash_delete(js_eventhandler->custom_events, ename);
            switch_mutex_unlock(js_eventhandler->mutex);

            JS_FreeCString(ctx, ename);
            continue;
        }
        if(switch_name_event(ename, &etype) == SWITCH_STATUS_SUCCESS) {
            if(etype == SWITCH_EVENT_CUSTOM) {
                custom = 1;
            }
            if(etype == SWITCH_EVENT_ALL) {
                for(j = 0; j < SWITCH_EVENT_ALL; j++) {
                    js_eventhandler->event_list[j] = SWITCH_FALSE;
                }
            } else {
                js_eventhandler->event_list[etype] = SWITCH_FALSE;
            }
            success++;
        }
        JS_FreeCString(ctx, ename);
    }

    return (success ? JS_TRUE : JS_FALSE);
}

static JSValue js_eventhandler_add_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    JSValue err = JS_UNDEFINED;
    const char *hdr_name = NULL;
    const char *hdr_val = NULL;

    EH_SANITY_CHECK();

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    hdr_name = JS_ToCString(ctx, argv[0]);
    if(zstr(hdr_name)) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: hdrName");
        goto out;
    }

    hdr_val = JS_ToCString(ctx, argv[1]);
    if(zstr(hdr_val)) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: hdrValue");
        goto out;
    }

    switch_mutex_lock(js_eventhandler->mutex);

    if(!js_eventhandler->filters) {
        switch_event_create_plain(&js_eventhandler->filters, SWITCH_EVENT_CLONE);
    }
    switch_event_add_header_string(js_eventhandler->filters, SWITCH_STACK_BOTTOM, hdr_name, hdr_val);

    switch_mutex_unlock(js_eventhandler->mutex);

out:
    JS_FreeCString(ctx, hdr_name);
    JS_FreeCString(ctx, hdr_val);
    return (JS_IsUndefined(err) ? JS_TRUE : err);
}

static JSValue js_eventhandler_del_filter(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    JSValue err = JS_UNDEFINED;
    const char *hdr_name = NULL;

    EH_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    hdr_name = JS_ToCString(ctx, argv[0]);
    if(zstr(hdr_name)) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: hdrName");
        goto out;
    }

    switch_mutex_lock(js_eventhandler->mutex);

    if(!js_eventhandler->filters) {
        switch_event_create_plain(&js_eventhandler->filters, SWITCH_EVENT_CLONE);
    }

    if(!strcasecmp(hdr_name, "all")) {
        switch_event_destroy(&js_eventhandler->filters);
        switch_event_create_plain(&js_eventhandler->filters, SWITCH_EVENT_CLONE);
    } else {
            switch_event_del_header(js_eventhandler->filters, hdr_name);
    }

    switch_mutex_unlock(js_eventhandler->mutex);

out:
    JS_FreeCString(ctx, hdr_name);
    return (JS_IsUndefined(err) ? JS_TRUE : err);
}

static JSValue js_eventhandler_get_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    switch_event_t *pevent = NULL;
    uint32_t timeout = 0;
    void *pop = NULL;

    EH_SANITY_CHECK();

    if(argc > 0) {
        JS_ToUint32(ctx, &timeout, argv[0]);
    }

    if(timeout) {
        if(switch_queue_pop_timeout(js_eventhandler->event_queue, &pop, (timeout * 1000)) == SWITCH_STATUS_SUCCESS) {
            pevent = (switch_event_t *) pop;
        }
    } else {
        if(switch_queue_pop(js_eventhandler->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
            pevent = (switch_event_t *) pop;
        }
    }

    if(pevent) {
        return js_event_object_create(ctx, pevent);
    }

    return JS_UNDEFINED;
}

static JSValue js_eventhandler_send_event(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque2(ctx, this_val, js_eventhandler_class_id);
    JSValue err = JS_UNDEFINED;
    js_event_t *js_event = NULL;
    switch_core_session_t *session = NULL;
    const char *session_uuid = NULL;
    uint8_t success = 0;

    EH_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    js_event = JS_GetOpaque(argv[0], js_event_class_get_id());
    if(!js_event) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: event");
        goto out;
    }

    if(argc > 1) {
        if(JS_IsString(argv[1])) {
            session_uuid = JS_ToCString(ctx, argv[1]);
            if(zstr(session_uuid)) {
                err = JS_ThrowTypeError(ctx, "Invalid argument: sessionUUID");
                goto out;
            }
            session = switch_core_session_locate(session_uuid);
            if(!session) {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't lookup session: %s\n", session_uuid);
                goto out;
            }
        } else {
            js_session_t *jss = JS_GetOpaque(argv[1], js_seesion_class_get_id());
            if(!jss || !jss->session) {
                err = JS_ThrowTypeError(ctx, "Invalid argument: session");
                goto out;
            }
            session = jss->session;
        }
    }

    if(!session) {
        switch_event_fire(&js_event->event);
        success = 1;
    } else {
        if(switch_core_session_queue_private_event(session, &js_event->event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
            success = 1;
        }
    }
out:
    JS_FreeCString(ctx, session_uuid);
    if(!JS_IsUndefined(err)) { return err; }
    return(success ? JS_TRUE : JS_FALSE);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_eventhandler_class = {
    CLASS_NAME,
    .finalizer = js_eventhandler_finalizer,
};

static const JSCFunctionListEntry js_eventhandler_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("ready", js_eventhandler_property_get, js_eventhandler_property_set, PROP_READY),
    //
    JS_CFUNC_DEF("subscribe", 1, js_eventhandler_subscribe),
    JS_CFUNC_DEF("unsubscribe", 1, js_eventhandler_unsubscribe),
    JS_CFUNC_DEF("addFilter", 1, js_eventhandler_add_filter),
    JS_CFUNC_DEF("delFilter", 1, js_eventhandler_del_filter),
    JS_CFUNC_DEF("getEvent", 1, js_eventhandler_get_event),
    JS_CFUNC_DEF("sendEvent", 1, js_eventhandler_send_event),
};

static void js_eventhandler_finalizer(JSRuntime *rt, JSValue val) {
    js_eventhandler_t *js_eventhandler = JS_GetOpaque(val, js_eventhandler_class_id);
    switch_memory_pool_t *pool = (js_eventhandler ? js_eventhandler->pool : NULL);

    if(!js_eventhandler) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-eventhandler-finalizer: js_eventhandler=%p\n", js_eventhandler);

    if(js_eventhandler->custom_events) {
        switch_core_hash_destroy(&js_eventhandler->custom_events);
    }

    if(js_eventhandler->event_queue) {
        void *pop = NULL;
        while(switch_queue_trypop(js_eventhandler->event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
            switch_event_t *pevent = (switch_event_t *) pop;
            if(pevent) {
                switch_event_destroy(&pevent);
            }
        }
    }

    if(js_eventhandler->filters) {
        switch_event_destroy(&js_eventhandler->filters);
    }
    if(js_eventhandler->mutex) {
        switch_mutex_destroy(js_eventhandler->mutex);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_eventhandler);
}

static JSValue js_eventhandler_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_eventhandler_t *js_eventhandler = NULL;
    switch_memory_pool_t *pool = NULL;

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    switch_mutex_init(&js_eventhandler->mutex, SWITCH_MUTEX_NESTED, pool);
    switch_queue_create(&js_eventhandler->event_queue, MAX_QUEUE_LEN, pool);
    switch_core_hash_init(&js_eventhandler->custom_events);

    js_eventhandler->filters = NULL;
    memset(&js_eventhandler->event_list, 0, sizeof(js_eventhandler->event_list));

    js_eventhandler = js_mallocz(ctx, sizeof(js_eventhandler_t));
    if(!js_eventhandler) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }
    js_eventhandler->pool = pool;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_eventhandler_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_eventhandler);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-eventhandler-constructor: js_eventhandler=%p\n", js_eventhandler);

    return obj;
fail:
    if(js_eventhandler) {
        if(js_eventhandler->custom_events) {
            switch_core_hash_destroy(&js_eventhandler->custom_events);
        }
        if(js_eventhandler->filters) {
            switch_event_destroy(&js_eventhandler->filters);
        }
        if(js_eventhandler->mutex) {
            switch_mutex_destroy(js_eventhandler->mutex);
        }
        js_free(ctx, js_eventhandler);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    JS_FreeValue(ctx, obj);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_eventhandler_class_get_id() {
    return js_eventhandler_class_id;
}

switch_status_t js_eventhandler_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_eventhandler_class_id) {
        JS_NewClassID(&js_eventhandler_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_eventhandler_class_id, &js_eventhandler_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_eventhandler_proto_funcs, ARRAY_SIZE(js_eventhandler_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_eventhandler_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_eventhandler_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}
