/**
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME              "XML"


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_xml_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_xml_class_register(JSContext *ctx, JSValue global_obj) {




    return SWITCH_STATUS_SUCCESS;
}
