/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include "js_xml.h"

#define CLASS_NAME              "XML"
#define PROP_NAME                0
#define PROP_DATA                1
#define PROP_ERROR               2

#define XML_SANITY_CHECK() if (!js_xml || !js_xml->xml) { \
           return JS_ThrowTypeError(ctx, "XML not initialized"); \
        }

static void js_xml_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_xml_object_create(JSContext *ctx, switch_xml_t xml);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_xml_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));

    if(!js_xml) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_NAME: {
             if(js_xml->xml) {
                return JS_NewString(ctx, switch_xml_name(js_xml->xml));
             }
             return JS_UNDEFINED;
        }
        case PROP_DATA: {
             if(js_xml->xml) {
                return JS_NewString(ctx, switch_xml_txt(js_xml->xml));
             }
             return JS_UNDEFINED;
        }
        case PROP_ERROR: {
            if(js_xml->xml) {
                return JS_NewString(ctx, switch_xml_error(js_xml->xml));
            }
            return JS_UNDEFINED;
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_xml_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    const char *str = NULL;

    if(!js_xml || !js_xml->xml) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_DATA: {
            if(QJS_IS_NULL(val)) {
                switch_xml_set_txt(js_xml->xml, "");
            } else {
                str = JS_ToCString(ctx, val);
                switch_xml_set_txt_d(js_xml->xml, str);
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSValue js_xml_add_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    switch_xml_t xml = NULL;
    const char *name = NULL;

    XML_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: name");
    }

    name = JS_ToCString(ctx, argv[0]);

    xml = switch_xml_add_child_d(js_xml->xml, name, 0);
    if(!xml) {
        return JS_ThrowTypeError(ctx, "Couldn't create XML object");
    }

    JS_FreeCString(ctx, name);
    return js_xml_object_create(ctx, xml);
}

static JSValue js_xml_get_child(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    const char *name = NULL, *attr_name = NULL, *attr_val = NULL;
    switch_xml_t xml = NULL;
    JSValue result = JS_UNDEFINED;

    XML_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: name");
    }
    name = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        attr_name = (QJS_IS_NULL(argv[1]) ? NULL : JS_ToCString(ctx, argv[1]));
    }
    if(argc > 2) {
        attr_val = (QJS_IS_NULL(argv[2]) ? NULL : JS_ToCString(ctx, argv[2]));
    }

    if(name && !attr_name) {
        xml = switch_xml_child(js_xml->xml, name);
    } else if (name) {
        xml = switch_xml_find_child(js_xml->xml, name, attr_name, attr_val);
    }

    if(xml) {
        result = js_xml_object_create(ctx, xml);
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, attr_name);
    JS_FreeCString(ctx, attr_val);

    return result;
}

static JSValue js_xml_get_attr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    const char *name = NULL;
    const char *val = NULL;

    XML_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: name");
    }

    name = JS_ToCString(ctx, argv[0]);

    val = switch_xml_attr_soft(js_xml->xml, name);
    JS_FreeCString(ctx, name);

    if(val) {
        return JS_NewString(ctx, val);
    }
    return JS_UNDEFINED;
}

static JSValue js_xml_set_attr(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    const char *name = NULL, *value = NULL;

    XML_SANITY_CHECK();

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: name");
    }

    name = JS_ToCString(ctx, argv[0]);

    if(argc > 1) {
        value = JS_ToCString(ctx, argv[1]);
    }

    if(value) {
        switch_xml_set_attr_d(js_xml->xml, name, value);
    } else {
        switch_xml_set_attr(js_xml->xml, name, "");
    }

    JS_FreeCString(ctx, name);
    JS_FreeCString(ctx, value);

    return JS_TRUE;
}

static JSValue js_xml_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));

    XML_SANITY_CHECK();

    switch_xml_remove(js_xml->xml);

    return JS_TRUE;
}

static JSValue js_xml_copy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    js_xml_t *js_xml2 = NULL;
    switch_xml_t xml = NULL;
    JSValue obj;

    XML_SANITY_CHECK();

    xml = switch_xml_dup(js_xml->xml);
    if(!xml) {
        return JS_ThrowTypeError(ctx, "Couldn't create XML");
    }

    obj = js_xml_object_create(ctx, xml);
    js_xml2 = JS_GetOpaque2(ctx, obj, js_xml_get_classid(ctx));
    if(js_xml2) {
        js_xml2->fl_free_xml = 1;
    }

    return obj;
}

static JSValue js_xml_next(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    JSValue result = JS_UNDEFINED;
    switch_xml_t xml = NULL;

    xml = switch_xml_next(js_xml->xml);
    if(xml) {
        result = js_xml_object_create(ctx, xml);
    }

    return result;
}

static JSValue js_xml_serialize(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_xml_t *js_xml = JS_GetOpaque2(ctx, this_val, js_xml_get_classid(ctx));
    char *data = NULL;
    JSValue obj;

    XML_SANITY_CHECK();

    data = switch_xml_toxml(js_xml->xml, SWITCH_FALSE);
    if(data) {
        obj = JS_NewString(ctx, data);
        switch_safe_free(data);

        return obj;
    }

    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_xml_class = {
    CLASS_NAME,
    .finalizer = js_xml_finalizer,
};

static const JSCFunctionListEntry js_xml_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("name", js_xml_property_get, js_xml_property_set, PROP_NAME),
    JS_CGETSET_MAGIC_DEF("data", js_xml_property_get, js_xml_property_set, PROP_DATA),
    JS_CGETSET_MAGIC_DEF("error", js_xml_property_get, js_xml_property_set, PROP_ERROR),
    //
    JS_CFUNC_DEF("addChild", 1, js_xml_add_child),
    JS_CFUNC_DEF("getChild", 1, js_xml_get_child),
    JS_CFUNC_DEF("getAttribute", 1, js_xml_get_attr),
    JS_CFUNC_DEF("setAttribute", 1, js_xml_set_attr),
    JS_CFUNC_DEF("remove", 0, js_xml_remove),
    JS_CFUNC_DEF("copy", 0, js_xml_copy),
    JS_CFUNC_DEF("next", 0, js_xml_next),
    JS_CFUNC_DEF("serialize", 0, js_xml_serialize),
};

static void js_xml_finalizer(JSRuntime *rt, JSValue val) {
    js_xml_t *js_xml = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));

    if(!js_xml) {
        return;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-xml-finalizer: js_xml=%p, xml=%p, fl_free_xml=%i\n", js_xml, js_xml->xml, js_xml->fl_free_xml);
#endif

    if(js_xml->fl_free_xml) {
        if(js_xml->xml) {
            switch_xml_free(js_xml->xml);
            js_xml->xml = NULL;
        }
    }

    js_free_rt(rt, js_xml);
}

static JSValue js_xml_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_xml_t *js_xml = NULL;
    switch_xml_t xml = NULL;
    const char *data = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: data");
    }

    data = JS_ToCString(ctx, argv[0]);

    xml = switch_xml_parse_str_dynamic((char *)data, SWITCH_TRUE);
    if(!xml) {
        return JS_ThrowTypeError(ctx, "Couldn't create root node");
    }

    js_xml = js_mallocz(ctx, sizeof(js_xml_t));
    if(!js_xml) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        goto fail;
    }

    js_xml->xml = xml;
    js_xml->fl_free_xml = 1;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_xml_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_xml);
    JS_FreeCString(ctx, data);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-xml-constructor: js-xml=%p, xml=%p\n", js_xml, js_xml->xml);
#endif

    return obj;

fail:
    if(js_xml) {
        js_free(ctx, js_xml);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, data);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

static JSValue js_xml_object_create(JSContext *ctx, switch_xml_t xml) {
    js_xml_t *js_xml = NULL;
    JSValue obj, proto;

    js_xml = js_mallocz(ctx, sizeof(js_xml_t));
    if(!js_xml) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        return JS_EXCEPTION;
    }

    proto = JS_NewObject(ctx);
    if(JS_IsException(proto)) { return proto; }
    JS_SetPropertyFunctionList(ctx, proto, js_xml_proto_funcs, ARRAY_SIZE(js_xml_proto_funcs));

    obj = JS_NewObjectProtoClass(ctx, proto, js_xml_get_classid(ctx));
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_xml->xml = xml;
    js_xml->fl_free_xml = 0;

    JS_SetOpaque(obj, js_xml);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-xml-obj-created: js_xml=%p, xml=%p\n", js_xml, js_xml->xml);
#endif

    return obj;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_xml_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_xml_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_xml_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_xml_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_xml_proto_funcs, ARRAY_SIZE(js_xml_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_xml_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

