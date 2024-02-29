/**
 * (C)2024 aks
 * https://github.com/akscf/
 **/
#include "js_ivs.h"

uint32_t js_ivs_take(js_ivs_t *js_ivs) {
    uint32_t status = false;

    if(!js_ivs || js_ivs->fl_destroyed) {
        return false;
    }

    switch_mutex_lock(js_ivs->mutex);
    if(js_ivs->fl_ready) {
        status = true;
        js_ivs->wlock++;
    }
    switch_mutex_unlock(js_ivs->mutex);

    return status;
}

void js_ivs_release(js_ivs_t *js_ivs) {
    switch_assert(js_ivs);

    switch_mutex_lock(js_ivs->mutex);
    if(js_ivs->wlock) { js_ivs->wlock--; }
    switch_mutex_unlock(js_ivs->mutex);
}

uint32_t js_ivs_gen_job_id(js_ivs_t *js_ivs) {
    uint32_t ret = 0;

    if(!js_ivs || js_ivs->fl_destroyed) { return false; }

    switch_mutex_lock(js_ivs->mutex);

    if(js_ivs->jobs_seq == 0) { js_ivs->jobs_seq = 1; }
    ret = js_ivs->jobs_seq;
    js_ivs->jobs_seq++;

    switch_mutex_unlock(js_ivs->mutex);

    return ret;
}

int js_ivs_xflags_test(js_ivs_t *js_ivs, int flag) {
    int val = 0;
    switch_assert(js_ivs);

    switch_mutex_lock(js_ivs->mutex);
    val = BIT_CHECK(js_ivs->xflags, flag);
    switch_mutex_unlock(js_ivs->mutex);

    return val;
}

void js_ivs_xflags_set(js_ivs_t *js_ivs, int flag, int val) {
    switch_assert(js_ivs);

    switch_mutex_lock(js_ivs->mutex);
    if(val) { BIT_SET(js_ivs->xflags, flag); }
    else { BIT_CLEAR(js_ivs->xflags, flag);  }
    switch_mutex_unlock(js_ivs->mutex);
}

int js_ivs_xflags_test_unsafe(js_ivs_t *js_ivs, int flag) {
    switch_assert(js_ivs);

    return BIT_CHECK(js_ivs->xflags, flag);
}

void js_ivs_xflags_set_unsafe(js_ivs_t *js_ivs, int flag, int val) {
    switch_assert(js_ivs);

    if(val) { BIT_SET(js_ivs->xflags, flag); }
    else { BIT_CLEAR(js_ivs->xflags, flag);  }
}


const char *js_ivs_chunkType2name(uint32_t id) {
    switch(id) {
        case IVS_CHUNK_TYPE_BUFFER: return "buffer";
        case IVS_CHUNK_TYPE_FILE:   return "file";
    }
    return "buffer";
}

uint32_t js_ivs_chunkType2id(const char *name) {
    if(!zstr(name)) {
        if(strcasecmp(name, "buffer") == 0) {
            return IVS_CHUNK_TYPE_BUFFER;
        }
        if(strcasecmp(name, "file") == 0) {
            return IVS_CHUNK_TYPE_FILE;
        }
    }
    return IVS_CHUNK_TYPE_BUFFER;
}

const char *js_ivs_chunkEncoding2name(uint32_t id) {
    switch(id) {
        case IVS_CHUNK_ENCODING_WAV: return "wav";
        case IVS_CHUNK_ENCODING_MP3: return "mp3";
        case IVS_CHUNK_ENCODING_RAW: return "raw";
        case IVS_CHUNK_ENCODING_B64: return "b64";
    }
    return "raw";
}

uint32_t js_ivs_chunkEncoding2id(const char *name) {
    if(!zstr(name)) {
        if(strcasecmp(name, "wav") == 0) {
            return IVS_CHUNK_ENCODING_WAV;
        }
        if(strcasecmp(name, "mp3") == 0) {
            return IVS_CHUNK_ENCODING_MP3;
        }
        if(strcasecmp(name, "raw") == 0) {
            return IVS_CHUNK_ENCODING_RAW;
        }
        if(strcasecmp(name, "b64") == 0) {
            return IVS_CHUNK_ENCODING_B64;
        }
    }
    return IVS_CHUNK_ENCODING_RAW;
}

const char *js_ivs_vadState2name(switch_vad_state_t st) {
    switch(st) {
        case SWITCH_VAD_STATE_NONE:         return "none";
        case SWITCH_VAD_STATE_START_TALKING:return "start-talking";
        case SWITCH_VAD_STATE_STOP_TALKING: return "stop-talking";
        case SWITCH_VAD_STATE_TALKING:      return "talking";
        case SWITCH_VAD_STATE_ERROR:        return "error";
    }
    return "unknown";
}
