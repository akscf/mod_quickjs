/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"
extern globals_t globals;

static switch_status_t xxx_emit_event_chunk_ready(js_session_t *js_session, switch_buffer_t *chunk_buffer);
static switch_status_t xxx_emit_event_dtmf_ready(js_session_t *js_session, switch_buffer_t *dtmf_buffer);

static void *SWITCH_THREAD_FUNC ivs_audio_capture_thread(switch_thread_t *thread, void *obj) {
    volatile js_session_t *_ref = (js_session_t *) obj;
    js_session_t *js_session = (js_session_t *) _ref;
    switch_status_t status = 0;
    switch_memory_pool_t *pool = NULL;
    switch_vad_t *vad = NULL;
    switch_core_session_t *session = js_session->session;
    switch_codec_t *session_write_codec = switch_core_session_get_write_codec(session);
    switch_codec_t *session_read_codec = switch_core_session_get_read_codec(session);
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_buffer_t *chunk_buffer = NULL, *vad_buffer = NULL, *dtmf_buffer = NULL;
    switch_byte_t *cap_buffer = NULL;
    switch_frame_t *read_frame = NULL;
    switch_codec_implementation_t read_impl = { 0 };
    switch_frame_t write_frame = { 0 };
    switch_timer_t timer = { 0 };
    switch_vad_state_t vad_state = SWITCH_VAD_STATE_NONE;
    uint32_t cap_buffer_data_len = 0, chunk_buffer_size = 0, vad_buffer_size = 0, dtmf_buffer_size = 0;
    uint32_t vad_stored_frames = 0, dec_flags = 0, enc_flags = 0, dec_samplerate = 0, enc_samplerate = 0;
    uint8_t fl_has_audio = false, fl_skip_cng = false, fl_capture_on = false;
    time_t dtmf_timer = 0;
    void *pop = NULL;

    switch_core_session_get_read_impl(js_session->session, &read_impl);
    chunk_buffer_size = ((js_session->ivs_audio_chunk_sec * read_impl.samples_per_second) * read_impl.number_of_channels);
    vad_buffer_size = (js_session->decoded_packet_size * IVS_VAD_STORE_FRAMES);
    dtmf_buffer_size = (js_session->ivs_dtmf_max_digits > 128 ? js_session->ivs_dtmf_max_digits : 128);

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    if(switch_buffer_create(pool, &chunk_buffer, chunk_buffer_size) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    if(switch_buffer_create(pool, &vad_buffer, vad_buffer_size) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    if(switch_buffer_create(pool, &dtmf_buffer, dtmf_buffer_size) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    if((cap_buffer = switch_core_alloc(pool, SWITCH_RECOMMENDED_BUFFER_SIZE)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mem fail\n");
        goto out;
    }
    if((write_frame.data = switch_core_alloc(pool, SWITCH_RECOMMENDED_BUFFER_SIZE)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if((vad = switch_vad_init(js_session->samplerate, js_session->channels)) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "vad fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    switch_vad_set_mode(vad, -1);
    switch_vad_set_param(vad, "debug", js_session->ivs_vad_debug);
    if(js_session->ivs_vad_silence_ms > 0)  { switch_vad_set_param(vad, "silence_ms", js_session->ivs_vad_silence_ms); }
    if(js_session->ivs_vad_voice_ms > 0)    { switch_vad_set_param(vad, "voice_ms", js_session->ivs_vad_voice_ms); }
    if(js_session->ivs_vad_threshold > 0)   { switch_vad_set_param(vad, "thresh", js_session->ivs_vad_threshold); }

    if(switch_core_timer_init(&timer, "soft", js_session->ptime, js_session->samplerate, pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "timer fail\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    while(true) {
        if(globals.fl_shutdown || !js_session->fl_ready) {
            break;
        }
        if(js_session_xflags_test(js_session, IVS_XFLAG_AUDIO_CAP_STOP)) {
            break;
        }

        if(vad_state == SWITCH_VAD_STATE_STOP_TALKING && switch_buffer_inuse(chunk_buffer) > 0) {
            xxx_emit_event_chunk_ready(js_session, chunk_buffer);
            switch_buffer_zero(chunk_buffer);
        }

        fl_has_audio = false;
        fl_skip_cng = false;

        // DTMF
        if(js_session->ivs_dtmf_max_digits > 0) {
            if(dtmf_timer && dtmf_timer <= switch_epoch_time_now(NULL)) {
                if(switch_buffer_inuse(dtmf_buffer) >= 0) {
                    xxx_emit_event_dtmf_ready(js_session, dtmf_buffer);
                    switch_buffer_zero(dtmf_buffer);
                }
                dtmf_timer = 0;
            }
            if(switch_queue_trypop(js_session->ivs_dtmfq, &pop) == SWITCH_STATUS_SUCCESS) {
                ivs_audio_buffer_t *dbuff = (ivs_audio_buffer_t *)pop;
                if(dbuff && dbuff->len > 0) {
                    if(switch_buffer_inuse(dtmf_buffer) + dbuff->len >= dtmf_buffer_size) {
                        xxx_emit_event_dtmf_ready(js_session, dtmf_buffer);
                        switch_buffer_zero(dtmf_buffer);
                    }
                    switch_buffer_write(dtmf_buffer, dbuff->data, dbuff->len);

                    if(switch_buffer_inuse(dtmf_buffer) >= js_session->ivs_dtmf_max_digits) {
                        xxx_emit_event_dtmf_ready(js_session, dtmf_buffer);
                        switch_buffer_zero(dtmf_buffer);
                    } else {
                        dtmf_timer = (switch_epoch_time_now(NULL) + js_session->ivs_dtmf_idle_sec);
                    }
                }
                ivs_audio_buffer_free(&dbuff);
            }
            if(switch_channel_has_dtmf(channel)) {
                char dbuff[32] = { 0 };
                uint32_t len = switch_channel_dequeue_dtmf_string(channel, (char *)dbuff, sizeof(dbuff));
                if(len > 0) {
                    if(switch_buffer_inuse(dtmf_buffer) + len >= dtmf_buffer_size) {
                        xxx_emit_event_dtmf_ready(js_session, dtmf_buffer);
                        switch_buffer_zero(dtmf_buffer);
                    }
                    switch_buffer_write(dtmf_buffer, (char *)dbuff, len);

                    if(switch_buffer_inuse(dtmf_buffer) >= js_session->ivs_dtmf_max_digits) {
                        xxx_emit_event_dtmf_ready(js_session, dtmf_buffer);
                        switch_buffer_zero(dtmf_buffer);
                    } else {
                        dtmf_timer = (switch_epoch_time_now(NULL) + js_session->ivs_dtmf_idle_sec);
                    }
                }
            }
        }

        // audio source
        if(switch_queue_trypop(js_session->ivs_audioq, &pop) == SWITCH_STATUS_SUCCESS) {
            ivs_audio_buffer_t *abuff = (ivs_audio_buffer_t *)pop;
            if(abuff && abuff->len > 0) {
                if(abuff->len != js_session->decoded_packet_size) {
                    dec_flags = 0;
                    dec_samplerate = js_session->samplerate;
                    cap_buffer_data_len = SWITCH_RECOMMENDED_BUFFER_SIZE;

                    if(switch_core_codec_ready(session_read_codec)) {
                        status = switch_core_codec_decode(session_read_codec, NULL, abuff->data, abuff->len, abuff->samplerate, cap_buffer, &cap_buffer_data_len, &dec_samplerate, &dec_flags);
                        if(status == SWITCH_STATUS_SUCCESS && cap_buffer_data_len > 0) {
                            fl_has_audio = true;
                            fl_skip_cng = true;
                        }
                    }
                } else {
                    cap_buffer_data_len = abuff->len;
                    memcpy(cap_buffer, abuff->data, abuff->len);
                    fl_has_audio = true;
                    fl_skip_cng = true;
                }
            }
            ivs_audio_buffer_free(&abuff);
        }

        if(fl_has_audio) {
            goto frame_proc;
        }
        if(js_session_xflags_test(js_session, IVS_XFLAG_PLAYBACK)) {
            goto timer_next;
        }

        // read frame
        status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
        if(SWITCH_READ_ACCEPTABLE(status) && !switch_test_flag(read_frame, SFF_CNG) && read_frame->samples > 0) {
            if(switch_core_codec_ready(session_read_codec)) {
                dec_flags = 0;
                dec_samplerate = js_session->samplerate;
                cap_buffer_data_len = SWITCH_RECOMMENDED_BUFFER_SIZE;

                status = switch_core_codec_decode(session_read_codec, NULL, read_frame->data, read_frame->datalen, js_session->samplerate, cap_buffer, &cap_buffer_data_len, &dec_samplerate, &dec_flags);
                if(status == SWITCH_STATUS_SUCCESS && cap_buffer_data_len > 0) {
                    fl_has_audio = true;
                    fl_skip_cng = false;
                }
            }
        }

        // processing frame
        frame_proc:
        if(fl_has_audio) {
            if(js_session->ivs_vad_state == SWITCH_VAD_STATE_STOP_TALKING || (js_session->ivs_vad_state == vad_state && vad_state == SWITCH_VAD_STATE_NONE)) {
                if(cap_buffer_data_len <= js_session->decoded_packet_size) {
                    if(vad_stored_frames >= IVS_VAD_STORE_FRAMES) {
                        switch_buffer_zero(vad_buffer);
                        vad_stored_frames = 0;
                    }
                    switch_buffer_write(vad_buffer, cap_buffer, cap_buffer_data_len);
                    vad_stored_frames++;
                }
            }

            vad_state = switch_vad_process(vad, (int16_t *)cap_buffer, (cap_buffer_data_len / sizeof(int16_t)));
            if(vad_state == SWITCH_VAD_STATE_START_TALKING) {
                if(vad_state != js_session->ivs_vad_state) { ivs_event_push_simple(IVS_EVENTSQ(js_session), IVS_EVENT_SPEAKING_START, NULL); }
                js_session->ivs_vad_state = vad_state;
                fl_capture_on = true;
            } else if (vad_state == SWITCH_VAD_STATE_STOP_TALKING) {
                if(vad_state != js_session->ivs_vad_state) { ivs_event_push_simple(IVS_EVENTSQ(js_session), IVS_EVENT_SPEAKING_STOP, NULL); }
                js_session->ivs_vad_state = vad_state;
                fl_capture_on = false;
                switch_vad_reset(vad);
            } else if (vad_state == SWITCH_VAD_STATE_TALKING) {
                js_session->ivs_vad_state = vad_state;
                fl_capture_on = true;
            }

            if(fl_capture_on) {
                if(vad_state == SWITCH_VAD_STATE_START_TALKING && switch_buffer_inuse(vad_buffer) > 0) {
                    const void *ptr = NULL;
                    switch_size_t dlen = 0, ofs = 0;
                    uint32_t rframes = 0, rlen = 0;

                    if((dlen = switch_buffer_peek_zerocopy(vad_buffer, &ptr)) && ptr && dlen > 0) {
                        rframes = (vad_stored_frames >= IVS_VAD_RECOVERY_FRAMES ? IVS_VAD_RECOVERY_FRAMES : vad_stored_frames);
                        rlen = (rframes * js_session->decoded_packet_size);
                        ofs = (dlen - rlen);
                        if(ofs < 0) { ofs = 0; }

                        if((switch_buffer_inuse(chunk_buffer) + rlen) >= chunk_buffer_size)  {
                            xxx_emit_event_chunk_ready(js_session, chunk_buffer);
                            switch_buffer_zero(chunk_buffer);
                        }

                        switch_buffer_write(chunk_buffer, (void *)(ptr + ofs), rlen);
                        switch_buffer_zero(vad_buffer);
                        vad_stored_frames = 0;
                    }
                }

                if(switch_buffer_inuse(chunk_buffer) >= chunk_buffer_size) {
                    xxx_emit_event_chunk_ready(js_session, chunk_buffer);
                    switch_buffer_zero(chunk_buffer);
                }

                switch_buffer_write(chunk_buffer, cap_buffer, cap_buffer_data_len);

                fl_capture_on = false;
            }
        } /* fl_has_audio */

        // CNG
        if(!fl_skip_cng && js_session->ivs_cng_lvl > 0 && js_session_xflags_test(js_session, IVS_XFLAG_AUDIO_CAP_CNG_ON)) {
            cap_buffer_data_len = (js_session->decoded_packet_size / sizeof(int16_t));
            switch_generate_sln_silence((int16_t *) cap_buffer, cap_buffer_data_len, js_session->channels, js_session->ivs_cng_lvl);

            if(switch_core_codec_ready(session_write_codec)) {
                enc_flags = 0;
                enc_samplerate = js_session->samplerate;

                status = switch_core_codec_encode(session_write_codec, NULL, cap_buffer, cap_buffer_data_len, js_session->samplerate, write_frame.data, &write_frame.datalen, &enc_samplerate, &enc_flags);
                if(status == SWITCH_STATUS_SUCCESS && write_frame.datalen > 0)  {
                    write_frame.codec = session_write_codec;
                    write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
                    write_frame.samples = write_frame.datalen;
                    switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
                }
            }
        }

        timer_next:
        switch_core_timer_next(&timer);
    }
out:
    js_session_xflags_set(js_session, IVS_XFLAG_AUDIO_CAP_STOP, false);

    if(timer.interval) {
        switch_core_timer_destroy(&timer);
    }
    if(vad) {
        switch_vad_destroy(&vad);
    }
    if(dtmf_buffer) {
        switch_buffer_destroy(&dtmf_buffer);
    }
    if(vad_buffer) {
        switch_buffer_destroy(&vad_buffer);
    }
    if(chunk_buffer) {
        switch_buffer_destroy(&chunk_buffer);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }

    switch_mutex_lock(js_session->ivs_mutex);
    js_session->fl_audio_cap_cative = false;
    switch_mutex_unlock(js_session->ivs_mutex);

    js_session_release(js_session);
    thread_finished();

    return NULL;
}

static switch_status_t xxx_emit_event_chunk_ready(js_session_t *js_session, switch_buffer_t *chunk_buffer) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    const void *ptr = NULL;
    switch_size_t buf_len = 0;
    uint32_t buf_time = 0, chunk_type = 0, chunk_encoding = 0;

    switch_mutex_lock(js_session->ivs_mutex);
    chunk_type = js_session->ivs_audio_chunk_type;
    chunk_encoding = js_session->ivs_audio_chunk_encoding;
    switch_mutex_unlock(js_session->ivs_mutex);

    if((buf_len = switch_buffer_peek_zerocopy(chunk_buffer, &ptr)) && ptr && buf_len > 0) {
        buf_time = (buf_len / js_session->samplerate);

        if(chunk_type == IVS_CHUNK_TYPE_FILE) {
            char *ofname = audio_file_write((switch_byte_t *)ptr, buf_len, js_session->samplerate, js_session->channels, ivs_chunkEncoding2name(chunk_encoding));

            if(ofname != NULL) {
                ivs_event_push_audio_chunk_ready(IVS_EVENTSQ(js_session), js_session->samplerate, js_session->channels, buf_time, buf_len, ofname, strlen(ofname));
            } else {
                status = SWITCH_STATUS_FALSE;
            }
            switch_safe_free(ofname);

        } else if(chunk_type == IVS_CHUNK_TYPE_BUFFER) {
            if(chunk_encoding == IVS_CHUNK_ENCODING_RAW) {

                ivs_event_push_audio_chunk_ready(IVS_EVENTSQ(js_session), js_session->samplerate, js_session->channels, buf_time, buf_len, (switch_byte_t *)ptr, buf_len);

            } else if(chunk_encoding == IVS_CHUNK_ENCODING_B64) {
                switch_byte_t *b64_buffer = NULL;
                uint32_t b64_buffer_len = BASE64_ENC_SZ(buf_len);

                switch_malloc(b64_buffer, b64_buffer_len);
                if(switch_b64_encode((uint8_t *)ptr, buf_len, b64_buffer, b64_buffer_len) == SWITCH_STATUS_SUCCESS) {
                    ivs_event_push_audio_chunk_ready_zcopy(IVS_EVENTSQ(js_session), js_session->samplerate, js_session->channels, buf_time, buf_len, b64_buffer, b64_buffer_len);
                } else {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_b64_encode() failed\n");
                    switch_safe_free(b64_buffer);
                }

            } else {
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unsupported encoding type: %s\n", ivs_chunkEncoding2name(chunk_encoding));
            }
        }
    } else {
        status = SWITCH_STATUS_FALSE;
    }
out:
    return status;
}

static switch_status_t xxx_emit_event_dtmf_ready(js_session_t *js_session, switch_buffer_t *dtmf_buffer) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    const void *ptr = NULL;
    switch_size_t buf_len = 0;

    if((buf_len = switch_buffer_peek_zerocopy(dtmf_buffer, &ptr)) && ptr && buf_len > 0) {
        ivs_event_push(IVS_EVENTSQ(js_session), JID_NONE, IVS_EVENT_DTMF_BUFFER_READY, (switch_byte_t *)ptr, buf_len);
    }
out:
    return status;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t ivs_audio_start_capture(js_session_t *js_session) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    uint8_t fl_th_started = false;

    switch_mutex_lock(js_session->ivs_mutex);
    if(js_session->fl_audio_cap_cative) {
        switch_mutex_unlock(js_session->ivs_mutex); // !!!
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Capture already active\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }
    js_session->fl_audio_cap_cative = true;
    switch_mutex_unlock(js_session->ivs_mutex);

    if(js_session_take(js_session)) {
        launch_thread(switch_core_session_get_pool(js_session->session), ivs_audio_capture_thread, js_session);
        fl_th_started = true;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't lock the session\n");
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(fl_th_started) {
            js_session_xflags_set(js_session, IVS_XFLAG_AUDIO_CAP_STOP, true);
        } else {
            switch_mutex_lock(js_session->ivs_mutex);
            js_session->fl_audio_cap_cative = false;
            switch_mutex_unlock(js_session->ivs_mutex);

            js_session_release(js_session);
        }
    }
    return status;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t ivs_audio_buffer_alloc(ivs_audio_buffer_t **out, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels) {
    ivs_audio_buffer_t *buf = NULL;

    switch_zmalloc(buf, sizeof(ivs_audio_buffer_t));

    if(data_len) {
        switch_malloc(buf->data, data_len);
        switch_assert(buf->data);

        buf->len = data_len;
        buf->samplerate = samplerate;
        buf->channels = channels;

        memcpy(buf->data, data, data_len);
    }

    *out = buf;
    return SWITCH_STATUS_SUCCESS;
}

void ivs_audio_buffer_free(ivs_audio_buffer_t **buf) {
    if(buf && *buf) {
        switch_safe_free((*buf)->data);
        free(*buf);
    }
}

switch_status_t ivs_audio_frame_push(switch_queue_t *queue, switch_byte_t *data, uint32_t data_len, uint32_t samplerate, uint8_t channels) {
    ivs_audio_buffer_t *buff = NULL;

    if(ivs_audio_buffer_alloc(&buff, data, data_len, samplerate, channels) == SWITCH_STATUS_SUCCESS) {
        if(switch_queue_trypush(queue, buff) == SWITCH_STATUS_SUCCESS) {
            return SWITCH_STATUS_SUCCESS;
        }
        ivs_audio_buffer_free(&buff);
    }
    return SWITCH_STATUS_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
void ivs_audio_queues_clean(switch_queue_t *queue) {
    ivs_audio_buffer_t *data = NULL;
    if(queue) {
        while(switch_queue_trypop(queue, (void *) &data) == SWITCH_STATUS_SUCCESS) {
            if(data) { ivs_audio_buffer_free(&data); }
        }
    }
}

void ivs_audio_queue_term(js_session_t *js_session) {
    if(js_session->ivs_audioq) {
        ivs_audio_queues_clean(js_session->ivs_audioq);
        switch_queue_term(js_session->ivs_audioq);
    }
    if(js_session->ivs_dtmfq) {
        ivs_audio_queues_clean(js_session->ivs_dtmfq);
        switch_queue_term(js_session->ivs_dtmfq);
    }
}
