/**
 * (C)2025 aks
 * https://github.com/akscf/
 **/
#include <mod_quickjs.h>

extern globals_t globals;

#define PLAY_AND_DETECT_DONE 1
#define PLAY_AND_DETECT_DONE_RECOGNIZING 2
typedef struct {
    int done;
    char *result;
    switch_input_args_t *original_args;
} play_and_detect_speech_state_t;

static void deliver_asr_event(switch_core_session_t *session, switch_event_t *event, switch_input_args_t *args) {
    if (args && args->input_callback) {
        args->input_callback(session, (void *)event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);
    }
}

static switch_status_t play_and_detect_input_callback(switch_core_session_t *session, void *input, switch_input_type_t input_type, void *data, unsigned int len) {
    play_and_detect_speech_state_t *state = (play_and_detect_speech_state_t *)data;

    if (!state->done) {
        switch_channel_t *channel = switch_core_session_get_channel(session);
        if (input_type == SWITCH_INPUT_TYPE_EVENT) {
            switch_event_t *event;
            event = (switch_event_t *)input;

            if (event->event_id == SWITCH_EVENT_DETECTED_SPEECH) {
                const char *speech_type = switch_event_get_header(event, "Speech-Type");

                if (!zstr(speech_type)) {
                    deliver_asr_event(session, event, state->original_args);

                    if (!strcasecmp(speech_type, "detected-speech")) {
                        const char *result;

                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "(%s) DETECTED SPEECH\n", switch_channel_get_name(channel));
                        result = switch_event_get_body(event);
                        if (!zstr(result)) {
                            state->result = switch_core_session_strdup(session, result);
                        } else {
                            state->result = "";
                        }

                        state->original_args = NULL;
                        state->done = PLAY_AND_DETECT_DONE_RECOGNIZING;
                        return SWITCH_STATUS_BREAK;
                    } else if (!strcasecmp(speech_type, "detected-partial-speech")) {
                        // ok
                    } else if (!strcasecmp(speech_type, "begin-speaking")) {
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "(%s) START OF SPEECH\n", switch_channel_get_name(channel));
                        return SWITCH_STATUS_BREAK;
                    } else if (!strcasecmp("closed", speech_type)) {
                        state->done = PLAY_AND_DETECT_DONE_RECOGNIZING;
                        state->result = "";
                        return SWITCH_STATUS_BREAK;
                    } else {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "unhandled speech type %s\n", speech_type);
                    }
                }
            }
        } else if (input_type == SWITCH_INPUT_TYPE_DTMF) {
            if(state->original_args) {
                switch_input_args_t *myargs = state->original_args;

                if(myargs->input_callback) {
                    if(myargs->input_callback(session, input, input_type, myargs->buf, myargs->buflen) != SWITCH_STATUS_SUCCESS) {
                        state->done = PLAY_AND_DETECT_DONE;
                        return SWITCH_STATUS_BREAK;
                    }
                }
            }
        }
    }

    return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_play_and_detect_speech_ex(switch_core_session_t *session, const char *file, const char *mod_name, const char *grammar, char **result, uint32_t input_timeout, switch_input_args_t *args) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    int recognizing = 0;
    switch_input_args_t myargs = { 0 };
    play_and_detect_speech_state_t state = { 0, "", NULL };
    switch_channel_t *channel = switch_core_session_get_channel(session);

    arg_recursion_check_start(args);

    if (result == NULL) {
        goto done;
    }

    if (!input_timeout) input_timeout = 5000;

    /* start speech detection */
    if ((status = switch_ivr_detect_speech(session, mod_name, grammar, "", NULL, NULL)) != SWITCH_STATUS_SUCCESS) {
        /* map SWITCH_STATUS_FALSE to SWITCH_STATUS_GENERR to indicate grammar load failed SWITCH_STATUS_NOT_INITALIZED will be passed back to indicate ASR resource problem */
        if (status == SWITCH_STATUS_FALSE) {
            status = SWITCH_STATUS_GENERR;
        }
        goto done;
    }

    recognizing = 1;

    /* play the prompt, looking for detection result */
    if (args) {
        state.original_args = args;
        myargs.dmachine = args->dmachine;
        myargs.read_frame_callback = args->read_frame_callback;
        myargs.user_data = args->user_data;
    }

    myargs.input_callback = play_and_detect_input_callback;
    myargs.buf = &state;
    myargs.buflen = sizeof(state);

    if (file) {
        status = switch_ivr_play_file(session, NULL, file, &myargs);
    }

    if (args && args->dmachine && switch_ivr_dmachine_last_ping(args->dmachine) != SWITCH_STATUS_SUCCESS) {
        state.done |= PLAY_AND_DETECT_DONE;
        goto done;
    }

    if (status != SWITCH_STATUS_BREAK && status != SWITCH_STATUS_SUCCESS) {
        status = SWITCH_STATUS_FALSE;
        goto done;
    }

    /* wait for result if not done */
    if (!state.done) {
        switch_ivr_detect_speech_start_input_timers(session);
#ifdef MOD_QUICKJS_DEBUG
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "(%s) WAITING FOR RESULT\n", switch_channel_get_name(channel));
#endif

        while (!state.done && switch_channel_ready(channel)) {
            status = switch_ivr_sleep(session, input_timeout, SWITCH_FALSE, &myargs);

            if (args && args->dmachine && switch_ivr_dmachine_last_ping(args->dmachine) != SWITCH_STATUS_SUCCESS) {
                state.done |= PLAY_AND_DETECT_DONE;
                goto done;
            }

            if (status != SWITCH_STATUS_BREAK && status != SWITCH_STATUS_SUCCESS) {
                status = SWITCH_STATUS_FALSE;
                goto done;
            }
        }
    }

done:
    if (recognizing && !(state.done & PLAY_AND_DETECT_DONE_RECOGNIZING)) {
        switch_ivr_pause_detect_speech(session);
    }
    if (recognizing && switch_channel_var_true(channel, "play_and_detect_speech_close_asr")) {
        switch_ivr_stop_detect_speech(session);
    }

    if (state.done) {
        status = SWITCH_STATUS_SUCCESS;
    }
    if (result) {
        *result = state.result;
    }

    arg_recursion_check_stop(args);

    return status;
}
