/**
 * Event object
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME      "Event"
#define PROP_READY      0

#define EVENT_SANITY_CHECK() if (!js_event || !js_event->event) { \
           return JS_ThrowTypeError(ctx, "Event is not initialized"); \
        }

static void js_event_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_event_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    if(magic == PROP_READY) {
        uint8_t x = (js_event && js_event->event);
        return (x ? JS_TRUE : JS_FALSE);
    }

    if(!js_event || !js_event->event) {
        return JS_UNDEFINED;
    }

    return JS_UNDEFINED;
}

static JSValue js_event_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    return JS_FALSE;
}

static JSValue js_event_add_header(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));
    const char *hdr_name = NULL;
    const char *hdr_value = NULL;

    EVENT_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    hdr_name = JS_ToCString(ctx, argv[0]);
    if(zstr(hdr_name)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: headerName");
    }
    hdr_value = JS_ToCString(ctx, argv[1]);

    switch_event_add_header_string(js_event->event, SWITCH_STACK_BOTTOM, hdr_name, hdr_value);

    JS_FreeCString(ctx, hdr_name);
    JS_FreeCString(ctx, hdr_value);

    return JS_TRUE;
}

static JSValue js_event_get_header(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));
    const char *hdr_name = NULL;
    char *val = NULL;

    EVENT_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    hdr_name = JS_ToCString(ctx, argv[0]);
    if(zstr(hdr_name)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: headerName");
    }
    val = switch_event_get_header(js_event->event, hdr_name);

    JS_FreeCString(ctx, hdr_name);

    return JS_NewString(ctx, val);
}

static JSValue js_event_add_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));
    const char *body = NULL;

    EVENT_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    body = JS_ToCString(ctx, argv[0]);
    switch_event_add_body(js_event->event, "%s", body);

    JS_FreeCString(ctx, body);

    return JS_TRUE;
}

static JSValue js_event_get_body(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    EVENT_SANITY_CHECK();

    return JS_NewString(ctx, switch_event_get_body(js_event->event));
}

static JSValue js_event_get_type(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    EVENT_SANITY_CHECK();

    return JS_NewString(ctx, switch_event_name(js_event->event->event_id));
}

static JSValue js_event_serialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));
    JSValue result;
    char *buf = NULL;
    uint8_t type = 0;
    EVENT_SANITY_CHECK();

    if(argc > 0) {
        const char *fmt = JS_ToCString(ctx, argv[0]);
        if(!strcasecmp(fmt, "xml")) {
            type = 1;
        } else if(!strcasecmp(fmt, "json")) {
            type = 2;
        }
        JS_FreeCString(ctx, fmt);
    }

    if(type == 0) {
        if(switch_event_serialize(js_event->event, &buf, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
            result = JS_NewString(ctx, buf);
            switch_safe_free(buf);
            return result;
        }
    } else if(type == 1) {
        switch_xml_t xml = NULL;
        if((xml = switch_event_xmlize(js_event->event, SWITCH_VA_NONE))) {
            buf = switch_xml_toxml(xml, SWITCH_FALSE);
            result = JS_NewString(ctx, buf);
            switch_safe_free(buf);
            switch_xml_free(xml);
            return result;
        }
    } else if(type == 2) {
        if(switch_event_serialize_json(js_event->event, &buf) == SWITCH_STATUS_SUCCESS) {
            result = JS_NewString(ctx, buf);
            switch_safe_free(buf);
            return result;
        }
    }
    return JS_UNDEFINED;
}

static JSValue js_event_fire(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    EVENT_SANITY_CHECK();

    switch_event_fire(&js_event->event);
    switch_safe_free(js_event);

    return JS_TRUE;
}

static JSValue js_event_destroy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_event_t *js_event = JS_GetOpaque2(ctx, this_val, js_event_get_classid(ctx));

    return JS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_event_class = {
    CLASS_NAME,
    .finalizer = js_event_finalizer,
};

static const JSCFunctionListEntry js_event_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("isReady", js_event_property_get, js_event_property_set, PROP_READY),
    //
    JS_CFUNC_DEF("addHeader", 1, js_event_add_header),
    JS_CFUNC_DEF("getHeader", 1, js_event_get_header),
    JS_CFUNC_DEF("addBody", 1, js_event_add_body),
    JS_CFUNC_DEF("getBody", 1, js_event_get_body),
    JS_CFUNC_DEF("getType", 1, js_event_get_type),
    JS_CFUNC_DEF("serialize", 1, js_event_serialize),
    JS_CFUNC_DEF("fire", 1, js_event_fire),
    JS_CFUNC_DEF("destroy", 1, js_event_destroy),
};

static void js_event_finalizer(JSRuntime *rt, JSValue val) {
    js_event_t *js_event = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));

    if(!js_event) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-event-finalizer: js_event=%p, event=%p\n", js_event, js_event->event);

    if(js_event->event) {
        switch_event_destroy(&js_event->event);
    }

    js_free_rt(rt, js_event);
}

static JSValue js_event_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    js_event_t *js_event = NULL;
    switch_event_t *event = NULL;
    switch_event_types_t etype;

    js_event = js_mallocz(ctx, sizeof(js_event_t));
    if(!js_event) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    if(argc > 0) {
        const char *ename = JS_ToCString(ctx, argv[0]);
        if(ename) {
            if(switch_name_event(ename, &etype) != SWITCH_STATUS_SUCCESS) {
                obj = JS_ThrowTypeError(ctx, "Unknown event: %s", ename);
                JS_FreeCString(ctx, ename);
                return obj;
            }
        }
        JS_FreeCString(ctx, ename);

        if(etype == SWITCH_EVENT_CUSTOM) {
            const char *subclass_name = NULL;

            if(argc > 1) {
                subclass_name = JS_ToCString(ctx, argv[1]);
            } else {
                subclass_name = "none";
            }

            if(switch_event_create_subclass(&event, etype, subclass_name) != SWITCH_STATUS_SUCCESS) {
                obj = JS_ThrowTypeError(ctx, "Couldn't create event (subclass: %s)", subclass_name);
                JS_FreeCString(ctx, subclass_name);
                return obj;
            }
        } else {
            if(switch_event_create(&event, etype) != SWITCH_STATUS_SUCCESS) {
                return JS_ThrowTypeError(ctx, "Couldn't create event (type: %i)", etype);
            }
        }
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_event_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    js_event->event = event;
    JS_SetOpaque(obj, js_event);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-event-constructor: js-event=%p, event=%p\n", js_event, js_event->event);

    return obj;
fail:
    if(event) {
        switch_event_destroy(&js_event->event);
    }
    if(js_event) {
        js_free(ctx, js_event);
    }
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_event_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_event_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_event_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_event_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_event_proto_funcs, ARRAY_SIZE(js_event_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_event_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

JSValue js_event_object_create(JSContext *ctx, switch_event_t *event) {
    js_event_t *js_event;
    JSValue obj, proto;

    js_event = js_mallocz(ctx, sizeof(js_event_t));
    if(!js_event) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_event_proto_funcs, ARRAY_SIZE(js_event_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_event_get_classid(ctx));
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_event->event = event;
    JS_SetOpaque(obj, js_event);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-event-obj-created: js_event=%p\n", js_event);

    return obj;
}

