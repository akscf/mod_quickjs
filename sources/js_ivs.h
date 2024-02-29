/**
 * (C)2024 aks
 * https://github.com/akscf/
 **/
#ifndef JS_IVS_H
#define JS_IVS_H
#include "mod_quickjs.h"
#include "js_session.h"

#define BIT_SET(a,b)   ((a) |= (1UL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1UL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1UL<<(b))))

#define IVS_TIMERS_MAX                  10

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
#define IVS_EVENT_SILENCE_TIMEOUT       0x07
#define IVS_EVENT_TIMER_TIMEOUT         0x08
#define IVS_EVENT_SESSION_TIMEOUT       0x09
#define IVS_EVENT_TRANSCRIPTION_READY   0x10

#define IVS_XFLAG_PLAYBACK              0x01
#define IVS_XFLAG_AUDIO_CNG_ENABLED     0x02
#define IVS_XFLAG_AUDIO_CAP_ACTIVE      0x03
#define IVS_XFLAG_VIDEO_CAP_ACTIVE      0x04
#define IVS_XFLAG_AUDIO_CAP_PAUSE       0x05
#define IVS_XFLAG_VIDEO_CAP_PAUSE       0x06
#define IVS_XFLAG_AUDIO_CAP_DO_STOP     0x07
#define IVS_XFLAG_VIDEO_CAP_DO_STOP     0x08
#define IVS_XFLAG_TEHANDLER_INSTALLED   0x09
#define IVS_XFLAG_SRVC_THR_ACTIVE       0xF0
#define IVS_XFLAG_SRVC_THR_DO_STOP      0xF1

#define IVS_TIMER_REGL                  0x00
#define IVS_TIMER_ONCE                  0x01

#define IVS_AUDIOQ(ivs)     (ivs->audioq)
#define IVS_DTMFQ(ivs)      (ivs->dtmfq)
#define IVS_EVENTSQ(ivs)    (ivs->eventsq)

typedef struct {
    uint32_t    mode;
    uint32_t    interval;
    time_t      timer;

} js_ivs_timer_t;

typedef struct {
    switch_memory_pool_t    *pool;
    js_session_t            *js_session;
    switch_core_session_t   *fs_session;
    switch_mutex_t          *mutex;
    switch_mutex_t          *mutex_timers;
    switch_queue_t          *eventsq;
    switch_queue_t          *audioq;
    switch_queue_t          *dtmfq;
    switch_vad_state_t      vad_state;
    char                    *language;                  // context preferred language
    char                    *tts_engine;
    char                    *asr_engine;
    char                    *origin;
    js_ivs_timer_t          timers[IVS_TIMERS_MAX + 1]; //
    uint32_t                cng_lvl;
    uint32_t                xflags;
    uint32_t                wlock;                      // for async jobs
    uint32_t                jobs_seq;
    uint32_t                dtmf_max_digits;
    uint32_t                dtmf_idle_sec;              // sec
    uint32_t                audio_chunk_sec;            //
    uint32_t                audio_chunk_type;           // file, buffer,...
    uint32_t                audio_chunk_encoding;       // raw,wav,mp3,b64,..
    uint32_t                silence_timeout_sec;        // >0 sec
    uint32_t                session_timeout_sec;        // >0
    uint32_t                vad_voice_ms;
    uint32_t                vad_silence_ms;
    uint32_t                vad_threshold;
    uint8_t                 vad_debug;
    uint8_t                 fl_jss_need_unlock;
    uint8_t                 fl_fss_need_unlock;
    uint8_t                 fl_ready;
    uint8_t                 fl_destroyed;
} js_ivs_t;

JSClassID js_ivs_get_classid(JSContext *ctx);
switch_status_t js_ivs_class_register(JSContext *ctx, JSValue global_obj);

/* js_ivs_misc.c */
uint32_t js_ivs_gen_job_id(js_ivs_t *js_ivs);
uint32_t js_ivs_take(js_ivs_t *js_ivs);
void js_ivs_release(js_ivs_t *js_ivs);

int js_ivs_xflags_test(js_ivs_t *js_ivs, int flag);
void js_ivs_xflags_set(js_ivs_t *js_ivs, int flag, int val);
int js_ivs_xflags_test_unsafe(js_ivs_t *js_ivs, int flag);
void js_ivs_xflags_set_unsafe(js_ivs_t *js_ivs, int flag, int val);

const char *js_ivs_chunkType2name(uint32_t id);
uint32_t js_ivs_chunkType2id(const char *name);
const char *js_ivs_chunkEncoding2name(uint32_t id);
uint32_t js_ivs_chunkEncoding2id(const char *name);
const char *js_ivs_vadState2name(switch_vad_state_t st);

/* js_ivs_playback.c */
switch_status_t js_ivs_say(js_ivs_t *js_ivs, char *language, char *text, uint8_t async);
switch_status_t js_ivs_playback(js_ivs_t *js_ivs, char *path, uint8_t async);
switch_status_t js_ivs_playback_stop(js_ivs_t *js_ivs);
uint32_t js_ivs_async_playback(js_ivs_t *js_ivs, const char *path, uint8_t delete_file);
uint32_t js_ivs_async_say(js_ivs_t *js_ivs, const char *lang, const char *text);


/* js_ivs_audio.c */
typedef struct {
    uint32_t    samplerate;
    uint32_t    channels;
    uint32_t    len;
    uint8_t     *data;
} js_ivs_audio_buffer_t;
switch_status_t js_ivs_audio_start_capture(js_ivs_t *js_ivs);
switch_status_t js_ivs_audio_buffer_alloc(js_ivs_audio_buffer_t **out, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels);
switch_status_t js_ivs_audio_frame_push(switch_queue_t *queue, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels);

void js_ivs_audio_buffer_free(js_ivs_audio_buffer_t **buf);
void js_ivs_audio_queues_clean(switch_queue_t *queue);
void js_ivs_audio_queue_term(js_ivs_t *js_ivs);
void js_ivs_dtmf_queue_term(js_ivs_t *js_ivs);

/* js_ivs_event.c */
typedef void (js_ivs_event_payload_destroy_handler_t)(void *data);

typedef struct {
    uint32_t                                jid;
    uint32_t                                type;
    uint32_t                                payload_len;
    uint8_t                                 *payload;
    js_ivs_event_payload_destroy_handler_t  *payload_dh;
} js_ivs_event_t;

typedef struct {
    uint32_t        samplerate;
    uint32_t        channels;
    uint32_t        time;               // chunk size in sec (take into account the VAD fading)
    uint32_t        length;             // chunk size in bytes
    uint32_t        data_len;           // actual data length
    uint8_t         *data;              // samples
} js_ivs_event_payload_audio_chunk_t;

#define js_ivs_event_push(queue, jid, type, payload, payload_len) js_ivs_event_push_dh(queue, jid, type, payload, payload_len, NULL);
void js_ivs_event_free(js_ivs_event_t **evt);
void js_ivs_events_queue_clean(switch_queue_t *queue);
void js_ivs_events_queue_term(js_ivs_t *js_ivs);

switch_status_t js_ivs_event_push_simple(switch_queue_t *queue, uint32_t type, char *payload_str);
switch_status_t js_ivs_event_push_dh(switch_queue_t *queue, uint32_t jid, uint32_t type, void *payload, uint32_t payload_len, js_ivs_event_payload_destroy_handler_t *payload_dh);

switch_status_t js_ivs_event_push_audio_chunk_ready(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len);
switch_status_t js_ivs_event_push_audio_chunk_ready_zcopy(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len);

switch_status_t js_ivs_event_push_timer_timeout(switch_queue_t *queue, uint32_t timer_id);
switch_status_t js_ivs_event_push_session_timeout(switch_queue_t *queue);

/* js_ivs_srvc.c */
switch_status_t js_ivs_service_thread_start(js_ivs_t *js_ivs);


#endif

