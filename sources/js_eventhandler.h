/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_EVENTHANDLER_H
#define JS_EVENTHANDLER_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 event_list[SWITCH_EVENT_ALL + 1];
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_hash_t           *custom_events;
    switch_queue_t          *event_queue;
    switch_event_t          *filters;
} js_eventhandler_t;

JSClassID js_eventhandler_get_classid(JSContext *ctx);
JSClassID js_eventhandler_get_classid2(JSRuntime *rt);
switch_status_t js_eventhandler_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id);

#endif



