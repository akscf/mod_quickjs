/**
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <switch.h>
#include <switch_stun.h>
#include <switch_curl.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#define MOD_VERSION "1.0"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define JS_CURL_ENABLE 1

// -----------------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint32_t                id;
    uint32_t                tx_sem;
    const char              *session_id;
    char                    *args;
    void                    *script;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSValue                 int_handler;
} script_instance_t;

typedef struct {
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
    uint32_t                id;
    uint32_t                tx_sem;
    uint32_t                instances;
    switch_size_t           code_length;
    char                    *name;
    char                    *path;
    char                    *code;
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_inthash_t        *instances_map;
    switch_inthash_t        *classes_map;
} script_t;

void ctx_dump_error(script_t *script, script_instance_t *instance, JSContext *ctx);

// -----------------------------------------------------------------------------------------------------------------------------------------------
// Session
typedef struct {
    uint8_t                 fl_hup_auto;
    uint8_t                 fl_hup_hook;
    switch_core_session_t   *session;
    JSValue                 on_hangup;
    JSContext               *ctx;
    switch_byte_t           *frame_buffer;
    uint32_t                 frame_buffer_size;
} js_session_t;

JSClassID js_seesion_class_get_id();
switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session);
JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

// Codec
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
JSClassID js_codec_class_get_id();
switch_status_t js_codec_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_codec_from_session_wcodec(JSContext *ctx, switch_core_session_t *session);
JSValue js_codec_from_session_rcodec(JSContext *ctx, switch_core_session_t *session);

// DTMF
typedef struct {
    switch_dtmf_t           *dtmf;
} js_dtmf_t;

JSClassID js_dtmf_class_get_id();
switch_status_t js_dtmf_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_dtmf_object_create(JSContext *ctx, switch_dtmf_t *dtmf);

// Event
typedef struct {
    switch_event_t          *event;
} js_event_t;

JSClassID js_event_class_get_id();
switch_status_t js_event_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_event_object_create(JSContext *ctx, switch_event_t *event);

// FileHandle
typedef struct {
    uint8_t                 fl_closed;
    uint8_t                 fl_auto_close;
    switch_core_session_t   *session;
    switch_file_handle_t    *fh;
} js_file_handle_t;

JSClassID js_file_handle_class_get_id();
switch_status_t js_file_handle_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_file_handle_object_create(JSContext *ctx, switch_file_handle_t *fh, switch_core_session_t *session);

// FileIO
typedef struct {
    uint32_t                flags;
    int32_t                 bufsize;
    switch_size_t           buflen;
    char                    *path;
    char                    *buf;
    switch_file_t           *fd;
    switch_memory_pool_t    *pool;
} js_fileio_t;

JSClassID js_fileio_class_get_id();
switch_status_t js_fileio_class_register(JSContext *ctx, JSValue global_obj);

// File
typedef struct {
    uint8_t                 is_open;
    uint8_t                 tmp_file;
    uint8_t                 type;
    uint32_t                flags;
    switch_size_t           rdbuf_size;
    char                    *path;
    char                    *name;
    char                    *rdbuf;
    switch_file_t           *fd;
    switch_dir_t            *dir;
    switch_memory_pool_t    *pool;
} js_file_t;

JSClassID js_file_class_get_id();
switch_status_t js_file_class_register(JSContext *ctx, JSValue global_obj);

// Socket
typedef struct {
    uint8_t                 opened;
    uint8_t                 type;
    uint8_t                 nonblock;
    uint8_t                 mcttl;
    switch_sockaddr_t       *toaddr;
    switch_sockaddr_t       *loaddr;
    switch_size_t           buffer_size;
    switch_memory_pool_t    *pool;
    switch_socket_t         *socket;
    char                    *read_buffer;
} js_socket_t;
JSClassID js_socket_class_get_id();
switch_status_t js_socket_class_register(JSContext *ctx, JSValue global_obj);

// CoreDB
typedef struct {
    char                    *name;
    switch_core_db_t        *db;
    switch_core_db_stmt_t   *stmt;
    switch_memory_pool_t    *pool;
    JSContext               *ctx;
    JSValue                 callback;
} js_coredb_t;
JSClassID js_coredb_class_get_id();
switch_status_t js_coredb_class_register(JSContext *ctx, JSValue global_obj);

// EventHandler
typedef struct {
    uint8_t                 event_list[SWITCH_EVENT_ALL + 1];
    switch_memory_pool_t    *pool;
    switch_mutex_t          *mutex;
    switch_hash_t           *custom_events;
    switch_queue_t          *event_queue;
    switch_event_t          *filters;
} js_eventhandler_t;
JSClassID js_eventhandler_class_get_id();
switch_status_t js_eventhandler_class_register(JSContext *ctx, JSValue global_obj);

#ifdef JS_CURL_ENABLE
typedef struct {
    uint32_t                timeout;
    uint32_t                response_length;
    uint8_t                 fl_ignore_rdata;
    char                    *url;
    char                    *method;
    char                    *credentials;
    char                    *content_type;
    char                    *response_buffer;
    switch_memory_pool_t    *pool;
} js_curl_t;
JSClassID js_curl_class_get_id();
switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj);
#endif // JS_CURL_ENABLE

#endif
