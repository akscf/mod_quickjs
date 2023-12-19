/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

void ivs_events_queue_clean(switch_queue_t *queue) {
    ivs_event_t *data = NULL;
    if(queue) {
        while(switch_queue_trypop(queue, (void *) &data) == SWITCH_STATUS_SUCCESS) {
            if(data) { ivs_event_free(&data); }
        }
    }
}

void ivs_events_queue_term(js_session_t *js_session) {
    if(js_session->ivs_events) {
        ivs_events_queue_clean(js_session->ivs_events);
        switch_queue_term(js_session->ivs_events);
    }
}

void ivs_event_free(ivs_event_t **event) {
    ivs_event_t *event_ref = *event;

    if(event_ref) {
        if(event_ref->payload_dh) {
            event_ref->payload_dh(event_ref->payload);
        }
        switch_safe_free(event_ref->payload);
        switch_safe_free(event_ref);
        *event =  NULL;
    }
}

switch_status_t ivs_event_push_simple(switch_queue_t *queue, uint32_t type, char *payload_str) {
    ivs_event_t *event = NULL;

    switch_assert(queue);

    switch_zmalloc(event, sizeof(ivs_event_t));
    event->type = type;

    if(payload_str) {
        event->payload_len = strlen(payload_str);

        switch_malloc(event->payload, event->payload_len + 1);
        memcpy(event->payload, payload_str, event->payload_len);

        event->payload[event->payload_len] = '\0';
    }

    if(switch_queue_trypush(queue, event) == SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_SUCCESS;
    }

    ivs_event_free(&event);
    return SWITCH_STATUS_FALSE;
}

switch_status_t ivs_event_push_dh(switch_queue_t *queue, uint32_t jid, uint32_t type, void *payload, uint32_t payload_len, ivs_event_payload_destroy_handler_t *payload_dh) {
    ivs_event_t *event = NULL;

    switch_assert(queue);

    switch_zmalloc(event, sizeof(ivs_event_t));
    event->jid = jid;
    event->type = type;
    event->payload_dh = payload_dh;
    event->payload_len = payload_len;

    if(payload_len) {
        switch_malloc(event->payload, payload_len);
        memcpy(event->payload, payload, payload_len);
    }

    if(switch_queue_trypush(queue, event) == SWITCH_STATUS_SUCCESS) {
        return SWITCH_STATUS_SUCCESS;
    }

    ivs_event_free(&event);
    return SWITCH_STATUS_FALSE;

}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// audio-chunk
static void ivs_event_payload_free_audio_chunk(ivs_event_payload_audio_chunk_t *chunk) {
    if(chunk) {
        switch_safe_free(chunk->data);
    }
}

switch_status_t ivs_event_push_audio_chunk_ready(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len) {
    ivs_event_payload_audio_chunk_t *chunk = NULL;

    switch_zmalloc(chunk, sizeof(ivs_event_payload_audio_chunk_t));

    chunk->time = time;
    chunk->length = length;
    chunk->channels = channels;
    chunk->samplerate = samplerate;
    chunk->data_len = data_len;

    if(data_len) {
        switch_malloc(chunk->data, data_len);
        memcpy(chunk->data, data, data_len);
    }

    return ivs_event_push_dh(queue, JID_NONE, IVS_EVENT_AUDIO_CHUNK_READY, chunk, sizeof(ivs_event_payload_audio_chunk_t), (ivs_event_payload_destroy_handler_t *)ivs_event_payload_free_audio_chunk);
}

switch_status_t ivs_event_push_audio_chunk_ready_zcopy(switch_queue_t *queue, uint32_t samplerate, uint32_t channels, uint32_t time, uint32_t length, switch_byte_t *data, uint32_t data_len) {
    ivs_event_payload_audio_chunk_t *chunk = NULL;

    switch_zmalloc(chunk, sizeof(ivs_event_payload_audio_chunk_t));

    chunk->time = time;
    chunk->length = length;
    chunk->channels = channels;
    chunk->samplerate = samplerate;
    chunk->data_len = data_len;
    chunk->data = data;

    return ivs_event_push_dh(queue, JID_NONE, IVS_EVENT_AUDIO_CHUNK_READY, chunk, sizeof(ivs_event_payload_audio_chunk_t), (ivs_event_payload_destroy_handler_t *)ivs_event_payload_free_audio_chunk);
}

