// Teletone
typedef struct {
    uint8_t                         fl_dtmf_hook;
    char                            *timer_name;
    switch_buffer_t                 *audio_buffer;
    switch_memory_pool_t            *pool;
    switch_core_session_t           *session;
    switch_timer_t                  *timer_ptr;
    switch_codec_t                  *codec_ptr;
    teletone_generation_session_t   *tgs_ptr;
    teletone_generation_session_t   tgs;
    switch_codec_t                  codec;
    switch_timer_t                  timer;
    JSValue                         dtmf_cb;
    JSValue                         cb_arg;
} js_teletone_t;
JSClassID js_teletone_class_get_id();
switch_status_t js_teletone_class_register(JSContext *ctx, JSValue global_obj);

