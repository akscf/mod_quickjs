/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_EVENT_H
#define JS_EVENT_H
#include "mod_quickjs.h"

typedef struct {
    switch_event_t          *event;
} js_event_t;

JSClassID js_event_get_classid(JSContext *ctx);
switch_status_t js_event_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_event_object_create(JSContext *ctx, switch_event_t *event);


#endif


