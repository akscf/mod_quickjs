/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

const char *ivs_chunkType2name(uint32_t id) {
    switch(id) {
        case IVS_CHUNK_TYPE_BUFFER: return "buffer";
        case IVS_CHUNK_TYPE_FILE:   return "file";
    }
    return "buffer";
}

uint32_t ivs_chunkType2id(const char *name) {
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

const char *ivs_chunkEncoding2name(uint32_t id) {
    switch(id) {
        case IVS_CHUNK_ENCODING_WAV: return "wav";
        case IVS_CHUNK_ENCODING_MP3: return "mp3";
        case IVS_CHUNK_ENCODING_RAW: return "raw";
        case IVS_CHUNK_ENCODING_B64: return "b64";
    }
    return "raw";
}

uint32_t ivs_chunkEncoding2id(const char *name) {
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

const char *ivs_vadState2name(switch_vad_state_t st) {
    switch(st) {
        case SWITCH_VAD_STATE_NONE:         return "none";
        case SWITCH_VAD_STATE_START_TALKING:return "start-talking";
        case SWITCH_VAD_STATE_STOP_TALKING: return "stop-talking";
        case SWITCH_VAD_STATE_TALKING:      return "talking";
        case SWITCH_VAD_STATE_ERROR:        return "error";
    }
    return "unknown";
}
