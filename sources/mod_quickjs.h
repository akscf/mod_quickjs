/**
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/
#ifndef MOD_QUICKJS_H
#define MOD_QUICKJS_H

#include <switch.h>
#include <switch_stun.h>
#include <switch_curl.h>
#include <quickjs.h>
#include <quickjs-libc.h>

#include <sql.h>
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201)
#include <sqlext.h>
#pragma warning(pop)
#else
#include <sqlext.h>
#endif
#include <sqltypes.h>

#define MOD_VERSION "1.0"
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define FS_MOD_ENABLE_CURL
//#define FS_MOD_CURL_PROXY_SSL_OPTS_ENABLE
//#define FS_MOD_ENABLE_ODBC

// -----------------------------------------------------------------------------------------------------------------------------------------------
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

JSClassID js_seesion_get_classid(JSContext *ctx);
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
JSClassID js_codec_get_classid(JSContext *ctx);
switch_status_t js_codec_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_codec_from_session_wcodec(JSContext *ctx, switch_core_session_t *session);
JSValue js_codec_from_session_rcodec(JSContext *ctx, switch_core_session_t *session);

// DTMF
typedef struct {
    switch_dtmf_t           *dtmf;
} js_dtmf_t;

JSClassID js_dtmf_get_classid(JSContext *ctx);
switch_status_t js_dtmf_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_dtmf_object_create(JSContext *ctx, switch_dtmf_t *dtmf);

// Event
typedef struct {
    switch_event_t          *event;
} js_event_t;

JSClassID js_event_get_classid(JSContext *ctx);
switch_status_t js_event_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_event_object_create(JSContext *ctx, switch_event_t *event);

// FileHandle
typedef struct {
    uint8_t                 fl_closed;
    uint8_t                 fl_auto_close;
    switch_core_session_t   *session;
    switch_file_handle_t    *fh;
} js_file_handle_t;

JSClassID js_file_handle_get_classid(JSContext *ctx);
switch_status_t js_file_handle_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_file_handle_object_create(JSContext *ctx, switch_file_handle_t *fh, switch_core_session_t *session);

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

JSClassID js_file_get_classid(JSContext *ctx);
switch_status_t js_file_class_register(JSContext *ctx, JSValue global_obj);

// Socket
typedef struct {
    uint8_t                 opened;
    uint8_t                 type;
    uint8_t                 nonblock;
    uint8_t                 mcttl;
    uint32_t                timeout;
    switch_sockaddr_t       *toaddr;
    switch_sockaddr_t       *loaddr;
    switch_size_t           buffer_size;
    switch_memory_pool_t    *pool;
    switch_socket_t         *socket;
    char                    *read_buffer;
} js_socket_t;
JSClassID js_socket_get_classid(JSContext *ctx);
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
JSClassID js_coredb_get_classid(JSContext *ctx);
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
JSClassID js_eventhandler_get_classid(JSContext *ctx);
switch_status_t js_eventhandler_class_register(JSContext *ctx, JSValue global_obj);

// XML
typedef struct {
    uint8_t                 fl_free_xml;
    switch_xml_t            xml;
} js_xml_t;
JSClassID js_xml_get_classid(JSContext *ctx);
switch_status_t js_xml_class_register(JSContext *ctx, JSValue global_obj);

// cURL
#ifdef FS_MOD_ENABLE_CURL
typedef struct {
    uint32_t                timeout;
    uint32_t                response_length;
    uint8_t                 fl_ignore_rdata;
    uint8_t                 fl_ssl_verfypeer;
    uint8_t                 fl_ssl_verfyhost;
    char                    *url;
    char                    *proxy;
    char                    *cacert;
    char                    *cacert_proxy;
    char                    *method;
    char                    *credentials;
    char                    *content_type;
    char                    *response_buffer;
    char                    *credentials_proxy;
    switch_memory_pool_t    *pool;
} js_curl_t;
JSClassID js_curl_get_classid(JSContext *ctx);
switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj);
#endif // FS_MOD_ENABLE_CURL

// ODBC
#ifdef FS_MOD_ENABLE_ODBC
typedef struct {
    char                    *dsn;
    char                    *username;
    char                    *password;
    switch_memory_pool_t    *pool;
    JSContext               *ctx;
    switch_odbc_handle_t    *db;
    SQLCHAR                 *colbuf;
    uint32_t                colbufsz;
    SQLHSTMT                stmt;
} js_odbc_t;
JSClassID js_odbc_get_classid(JSContext *ctx);
switch_status_t js_odbc_class_register(JSContext *ctx, JSValue global_obj);
#endif // FS_MOD_ENABLE_ODBC

#endif
