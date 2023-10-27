/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_SESSION_H
#define JS_SESSION_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 fl_hup_auto;
    uint8_t                 fl_hup_hook;
    switch_core_session_t   *session;
    JSValue                 on_hangup;
    JSContext               *ctx;
    switch_byte_t           *frame_buffer;
    uint32_t                 frame_buffer_size;
} js_session_t;

JSClassID js_seesion_get_classid(JSContext *ctx);
switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session);
JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

#endif

