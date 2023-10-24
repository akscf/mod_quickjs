/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_DTMF_H
#define JS_DTMF_H
#include "mod_quickjs.h"

typedef struct {
    switch_dtmf_t           *dtmf;
} js_dtmf_t;

JSClassID js_dtmf_get_classid(JSContext *ctx);
switch_status_t js_dtmf_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_dtmf_object_create(JSContext *ctx, switch_dtmf_t *dtmf);


#endif

