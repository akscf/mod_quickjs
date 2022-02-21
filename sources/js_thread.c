/**
 * Threads
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME          "Thread"
#define PROP_ID             0

static void js_thread_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_thread_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_thread_t *js_thread = JS_GetOpaque2(ctx, this_val, js_thread_get_classid(ctx));
    JSValue ret = JS_UNDEFINED;

    if(!js_thread) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_ID: {
            if(js_thread_sem_take(js_thread)) {
                ret = JS_NewInt32(ctx, js_thread->id);
                js_thread_sem_release(js_thread);
            }
            return ret;
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_thread_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    return JS_FALSE;
}

static JSValue js_thread_is_interrupted(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_thread_t *js_thread = JS_GetOpaque2(ctx, this_val, js_thread_get_classid(ctx));
    JSValue ret = JS_UNDEFINED;

    if(js_thread_sem_take(js_thread)) {
        ret = (js_thread->fl_interrupted ? JS_TRUE : JS_FALSE);
        js_thread_sem_release(js_thread);
    }

    return ret;
}

static JSValue js_thread_is_alive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_thread_t *js_thread = JS_GetOpaque2(ctx, this_val, js_thread_get_classid(ctx));
    JSValue ret = JS_UNDEFINED;

    if(js_thread_sem_take(js_thread)) {
        ret = ((js_thread->fl_ready && !js_thread->fl_destroyed) ? JS_TRUE : JS_FALSE);
        js_thread_sem_release(js_thread);
    }

    return ret;
}

static JSValue js_thread_interrupt(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_thread_t *js_thread = JS_GetOpaque2(ctx, this_val, js_thread_get_classid(ctx));

    if(js_thread_sem_take(js_thread)) {
        if(!js_thread->fl_interrupted) {
            js_thread->fl_interrupted = SWITCH_TRUE;
        }
        js_thread_sem_release(js_thread);
        return JS_TRUE;
    }

    return JS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_thread_class = {
    CLASS_NAME,
    .finalizer = js_thread_finalizer,
};

static const JSCFunctionListEntry js_thread_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("id", js_thread_property_get, js_thread_property_set, PROP_ID),
    //
    JS_CFUNC_DEF("isAlive", 1, js_thread_is_alive),
    JS_CFUNC_DEF("isInterrupted", 1, js_thread_is_interrupted),
    JS_CFUNC_DEF("interrupt", 0, js_thread_interrupt),
};

static void js_thread_finalizer(JSRuntime *rt, JSValue val) {
    js_thread_t *js_thread = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));
    uint32_t id = 0;

    if(!js_thread) {
        return;
    }

    if(js_thread_sem_take(js_thread)) {
        id = js_thread->id;
        js_thread_sem_release(js_thread);
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-thread-finalizer: js_thread=%p, id=%i\n", js_thread, id);
}

static JSValue js_thread_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    return JS_ThrowTypeError(ctx, "Not available (use 'spawn' method)");
}

JSValue js_thread_object_create(JSContext *ctx, js_thread_t *js_thread) {
    JSValue obj, proto;

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_thread_proto_funcs, ARRAY_SIZE(js_thread_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_thread_get_classid(ctx));
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }
    JS_SetOpaque(obj, js_thread);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-thread-obj-created: js_thread=%p, id=%i\n", js_thread, js_thread->id);

    return obj;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_thread_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_thread_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_thread_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_thread_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_thread_proto_funcs, ARRAY_SIZE(js_thread_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_thread_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

