/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_SESSION_H
#define JS_SESSION_H
#include "mod_quickjs.h"

typedef struct {
    const char              *session_id;
    switch_byte_t           *frame_buffer;
    switch_core_session_t   *session;
    JSContext               *ctx;
    switch_mutex_t          *mutex;
    switch_file_handle_t    *bg_stream_fh;
    JSValue                 on_hangup;
    switch_call_cause_t     originate_fail_code;
    uint8_t                 fl_originate_fail_result;
    uint8_t                 fl_hup_auto;
    uint8_t                 fl_hup_hook;
    uint8_t                 fl_no_unlock;
    uint8_t                 fl_ready;
    uint32_t                wlock;
    uint32_t                frame_buffer_size;      // for direct r/w
    uint32_t                samplerate;
    uint32_t                channels;
    uint32_t                ptime;
    uint32_t                encoded_frame_size;    // bytes
    uint32_t                decoded_frame_size;    // bytes
    uint32_t                bg_streams;
} js_session_t;

/* js_session.c */
JSClassID js_seesion_get_classid(JSContext *ctx);
switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session);
JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* js_session_misc.c */
uint32_t js_session_take(js_session_t *session);
void js_session_release(js_session_t *session);

/* js_session_asr.c */
SWITCH_DECLARE(switch_status_t) switch_ivr_play_and_detect_speech_ex(switch_core_session_t *session, const char *file, const char *mod_name, const char *grammar, char **result, uint32_t timeout, switch_input_args_t *args);

/* js_session_bgs.c */
switch_status_t js_session_bgs_stream_start(js_session_t *js_session, const char *path);
switch_status_t js_session_bgs_stream_stop(js_session_t *js_session);

#endif

