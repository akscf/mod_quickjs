/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include "js_dtmf.h"

#define CLASS_NAME                 "DTMF"
#define PROP_DIGIT                 0
#define PROP_DURATION              1

static JSValue js_dtmf_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static void js_dtmf_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_dtmf_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_dtmf_t *js_dtmf = JS_GetOpaque2(ctx, this_val, js_dtmf_get_classid(ctx));

    if(!js_dtmf || !js_dtmf->dtmf) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_DIGIT: {
            char tmp[2] = { js_dtmf->dtmf->digit, '\0' };
            return JS_NewString(ctx, (char *) tmp);
        }
        case PROP_DURATION: {
            return JS_NewInt32(ctx, js_dtmf->dtmf->duration);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_dtmf_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_dtmf_t *js_dtmf = JS_GetOpaque2(ctx, this_val, js_dtmf_get_classid(ctx));

    return JS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_dtmf_class = {
    CLASS_NAME,
    .finalizer = js_dtmf_finalizer,
};

static const JSCFunctionListEntry js_dtmf_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("digit", js_dtmf_property_get, js_dtmf_property_set, PROP_DIGIT),
    JS_CGETSET_MAGIC_DEF("duration", js_dtmf_property_get, js_dtmf_property_set, PROP_DURATION),
};

static void js_dtmf_finalizer(JSRuntime *rt, JSValue val) {
    js_dtmf_t *js_dtmf = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));

    if(!js_dtmf) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-dtmf-finalizer: js_dtmf=%p\n", js_dtmf);

    /*if(js_dtmf->dtmf) {
        switch_safe_free(js_dtmf->dtmf);
    }*/

    js_free_rt(rt, js_dtmf);
}

static JSValue js_dtmf_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    js_dtmf_t *js_dtmf = NULL;
    int duration = 0;

    js_dtmf = js_mallocz(ctx, sizeof(js_dtmf_t));
    if(!js_dtmf) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    if(argc > 0) {
        JS_ToUint32(ctx, &duration, argv[0]);
    }
    if(duration <= 0) {
        duration = switch_core_default_dtmf_duration(0);
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_dtmf_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_dtmf);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-dtmf-constructor: js-dtmf=%p\n", js_dtmf);

    return obj;
fail:
    if(js_dtmf) {
        js_free(ctx, js_dtmf);
    }
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_dtmf_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_dtmf_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_dtmf_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_dtmf_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_dtmf_proto_funcs, ARRAY_SIZE(js_dtmf_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_dtmf_contructor, CLASS_NAME, 2, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);
    return SWITCH_STATUS_SUCCESS;
}

JSValue js_dtmf_object_create(JSContext *ctx, switch_dtmf_t *dtmf) {
    js_dtmf_t *js_dtmf;
    JSValue obj, proto;

    js_dtmf = js_mallocz(ctx, sizeof(js_dtmf_t));
    if(!js_dtmf) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_dtmf_proto_funcs, ARRAY_SIZE(js_dtmf_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_dtmf_get_classid(ctx));
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_dtmf->dtmf = dtmf;
    JS_SetOpaque(obj, js_dtmf);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-dtmf-obj-created: js_dtmf=%p\n", js_dtmf);

    return obj;
}


