/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <switch.h>
#include <switch_stun.h>
#include <switch_curl.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#include <js_xml.h>
#include <js_dtmf.h>
#include <js_odbc.h>
#include <js_coredb.h>
#include <js_codec.h>
#include <js_file.h>
#include <js_socket.h>
#include <js_event.h>
#include <js_coredb.h>
#include <js_filehandle.h>
#include <js_eventhandler.h>
#include <js_session.h>
#include <js_curl.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define QJS_IS_NULL(jsV)  (JS_IsNull(jsV) || JS_IsUndefined(jsV) || JS_IsUninitialized(jsV))

#define MOD_VERSION "1.4 (a1)"

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint8_t                 fl_interrupt;
    switch_size_t           script_len;
    uint32_t                sem;
    char                    *id;
    char                    *name;
    char                    *path;
    char                    *script_buf;
    char                    *args;
    const char              *session_id;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_classes_map;
    switch_hash_t           *classes_map;
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSRuntime               *rt;
    void                    *opaque;
} script_t;

void ctx_dump_error(script_t *script, JSContext *ctx);
switch_status_t js_register_classid(JSRuntime *rt, const char *class_name, JSClassID class_id);
JSClassID js_lookup_classid(JSRuntime *rt, const char *class_name);




#endif
