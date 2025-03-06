/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_FILEHANDLE_H
#define JS_FILEHANDLE_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 fl_closed;
    uint8_t                 fl_auto_close;
    switch_core_session_t   *session;
    switch_file_handle_t    *fh;
} js_file_handle_t;

JSClassID js_file_handle_get_classid(JSContext *ctx);
JSClassID js_file_handle_get_classid2(JSRuntime *rt);
switch_status_t js_file_handle_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id);

JSValue js_file_handle_object_create(JSContext *ctx, switch_file_handle_t *fh, switch_core_session_t *session);

#endif



