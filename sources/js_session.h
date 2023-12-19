/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_SESSION_H
#define JS_SESSION_H
#include "mod_quickjs.h"

#define IVS_EVENTS_QUEUE_SIZE           64
#define IVS_AUDIO_QUEUE_SIZE            64
#define IVS_DTMF_QUEUE_SIZE             64

#define IVS_VAD_STORE_FRAMES            64
#define IVS_VAD_RECOVERY_FRAMES         15

#define IVS_CHUNK_TYPE_FILE             0x01
#define IVS_CHUNK_TYPE_BUFFER           0x02

#define IVS_CHUNK_ENCODING_RAW          0x00
#define IVS_CHUNK_ENCODING_WAV          0x01
#define IVS_CHUNK_ENCODING_MP3          0x02
#define IVS_CHUNK_ENCODING_B64          0x03

#define IVS_EVENT_NONE                  0x00
#define IVS_EVENT_SPEAKING_START        0x01
#define IVS_EVENT_SPEAKING_STOP         0x02
#define IVS_EVENT_PLAYBACK_START        0x03
#define IVS_EVENT_PLAYBACK_STOP         0x04
#define IVS_EVENT_AUDIO_CHUNK_READY     0x05
#define IVS_EVENT_DTMF_BUFFER_READY     0x06

#define IVS_XFLAG_PLAYBACK              0x01
#define IVS_XFLAG_AUDIO_CAP_STOP        0x02
#define IVS_XFLAG_VIDEO_CAP_STOP        0x03
#define IVS_XFLAG_AUDIO_CAP_CNG_ON      0x04

typedef struct {
    const char              *session_id;
    switch_byte_t           *frame_buffer;
    switch_core_session_t   *session;
    JSContext               *ctx;
    JSValue                 on_hangup;
    uint8_t                 fl_hup_auto;
    uint8_t                 fl_hup_hook;
    uint8_t                 fl_no_unlock;
    uint8_t                 fl_ready;
    uint8_t                 fl_audio_cap_cative;
    uint8_t                 fl_video_cap_cative;
    uint32_t                frame_buffer_size;      // for direct r/w
    uint32_t                samplerate;
    uint32_t                channels;
    uint32_t                ptime;
    uint32_t                encoded_packet_size;    // bytes
    uint32_t                decoded_packet_size;    // bytes
    //
    switch_mutex_t          *ivs_mutex;
    switch_queue_t          *ivs_events;
    switch_queue_t          *ivs_audioq;
    switch_queue_t          *ivs_dtmfq;
    uint32_t                ivs_cng_lvl;
    uint32_t                ivs_xflags;
    uint32_t                ivs_wlock;                      // for async jobs
    uint32_t                ivs_jobs_seq;
    uint32_t                ivs_dtmf_max_digits;
    uint32_t                ivs_dtmf_idle_sec;              // sec
    uint32_t                ivs_audio_chunk_sec;            //
    uint32_t                ivs_audio_chunk_type;           // file, buffer,...
    uint32_t                ivs_audio_chunk_encoding;       // raw,wav,mp3,b64,..
    uint32_t                ivs_vad_voice_ms;
    uint32_t                ivs_vad_silence_ms;
    uint32_t                ivs_vad_threshold;
    uint8_t                 ivs_vad_debug;
    switch_vad_state_t      ivs_vad_state;
} js_session_t;


/* js_session.c */
JSClassID js_seesion_get_classid(JSContext *ctx);
switch_status_t js_session_class_register(JSContext *ctx, JSValue global_obj);
JSValue js_session_object_create(JSContext *ctx, switch_core_session_t *session);
JSValue js_session_ext_bridge(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv);

/* js_session_misc.c */
int js_session_xflags_test(js_session_t *session, int flag);
void js_session_xflags_set(js_session_t *session, int flag, int val);
uint32_t js_session_take(js_session_t *session);
void js_session_release(js_session_t *session);
uint32_t js_session_gen_job_id(js_session_t *session);

/* js_session_ivs_misc.c */
const char *ivs_chunkType2name(uint32_t id);
uint32_t ivs_chunkType2id(const char *name);
const char *ivs_chunkEncoding2name(uint32_t id);
uint32_t ivs_chunkEncoding2id(const char *name);
const char *ivs_vadState2name(switch_vad_state_t st);


/* js_session_playback.c */
switch_status_t ivs_say(js_session_t *session, char *language, char *text, uint8_t async);
switch_status_t ivs_playback(js_session_t *session, char *path, uint8_t async);
switch_status_t ivs_playback_stop(js_session_t *session);


/* js_session_ivs_wrp.c */
uint32_t ivs_async_playback(js_session_t *session, const char *path, uint8_t delete_file);
uint32_t ivs_async_say(js_session_t *session, const char *lang, const char *text);


/* js_session_ivs_audio.c */
#define IVS_AUDIOQ(session)     (session->ivs_audioq)
#define IVS_DTMFQ(session)     (session->ivs_dtmfq)

typedef struct {
    uint32_t    samplerate;
    uint32_t    channels;
    uint32_t    len;
    uint8_t     *data;
} ivs_audio_buffer_t;
switch_status_t ivs_audio_start_capture(js_session_t *js_session);
switch_status_t ivs_audio_buffer_alloc(ivs_audio_buffer_t **out, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels);
switch_status_t ivs_audio_frame_push(switch_queue_t *queue, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels);
void ivs_audio_buffer_free(ivs_audio_buffer_t **buf);
void ivs_audio_queues_clean(switch_queue_t *queue);
void ivs_audio_queue_term(js_session_t *session);


/* js_session_ivs_event.c */
#define IVS_EVENTSQ(session)     (session->ivs_events)
typedef void (ivs_event_payload_destroy_handler_t)(void *data);

typedef struct {
    uint32_t                            jid;
    uint32_t                            type;
    uint32_t                            payload_len;
    uint8_t                             *payload;
    ivs_event_payload_destroy_handler_t *payload_dh;
} ivs_event_t;

typedef struct {
    uint32_t        samplerate;
    uint32_t        channels;
    uint32_t        time;           // chunk size in sec
    uint32_t        length;         // chunk size in bytes
    uint32_t        data_len;       // actual data length
    uint8_t         *data;          // samples
} ivs_event_payload_audio_chunk_t;

#define ivs_event_push(queue, jid, type, payload, payload_len) ivs_event_push_dh(queue, jid, type, payload, payload_len, NULL);
void ivs_event_free(ivs_event_t **evt);
void ivs_events_queue_clean(switch_queue_t *queue);
void ivs_events_queue_term(js_session_t *js_session);

switch_status_t ivs_event_push_simple(switch_queue_t *queue, uint32_t type, char *payload_str);
switch_status_t ivs_event_push_dh(switch_queue_t *queue, uint32_t jid, uint32_t type, void *payload, uint32_t payload_len, ivs_event_payload_destroy_handler_t *payload_dh);

switch_status_t ivs_event_push_audio_chunk_ready(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len);
switch_status_t ivs_event_push_audio_chunk_ready_zcopy(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len);

#endif

