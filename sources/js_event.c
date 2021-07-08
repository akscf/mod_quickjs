/**
 * Event object
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define DTMF_CLASS_NAME                 "Event"

static JSClassID js_event_class_id;




// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_event_class_get_id() {
    return js_event_class_id;
}

void js_event_class_register_rt(JSRuntime *rt) {
    //JS_NewClassID(&js_event_class_id);
    //JS_NewClass(rt, js_event_class_id, &js_dtmf_class);
}

switch_status_t js_event_class_register_ctx(JSContext *ctx, JSValue global_obj) {

    return SWITCH_STATUS_SUCCESS;
}

/*JSValue js_event_object_create(JSContext *ctx, switch_dtmf_t *dtmf) {
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

    obj = JS_NewObjectProtoClass(ctx, proto, js_dtmf_class_id);
    JS_FreeValue(ctx, proto);

    if(JS_IsException(obj)) { return obj; }

    js_dtmf->dtmf = dtmf;
    JS_SetOpaque(obj, js_dtmf);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "object_created: js_dtmf=%p\n", js_dtmf);

    return obj;
}
*/
