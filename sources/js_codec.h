/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_CODEC_H
#define JS_CODEC_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 fl_can_destroy;
    uint32_t                samplerate;
    uint32_t                channels;
    uint32_t                ptime;
    uint32_t                flags;
    const char              *name;
    switch_memory_pool_t    *pool;
    switch_codec_t          *codec;
    switch_codec_t          codec_base;
} js_codec_t;

JSClassID js_codec_get_classid(JSContext *ctx);
switch_status_t js_codec_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_codec_from_session_wcodec(JSContext *ctx, switch_core_session_t *session);
JSValue js_codec_from_session_rcodec(JSContext *ctx, switch_core_session_t *session);


#endif


