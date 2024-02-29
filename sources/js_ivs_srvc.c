/**
 * (C)2024 aks
 * https://github.com/akscf/
 **/
#include "js_ivs.h"
extern globals_t globals;

static void *SWITCH_THREAD_FUNC js_ivs_service_thread(switch_thread_t *thread, void *obj) {
    volatile js_ivs_t *_ref = (js_ivs_t *) obj;
    js_ivs_t *js_ivs = (js_ivs_t *) _ref;
    js_ivs_timer_t *timers = (js_ivs_timer_t *)js_ivs->timers;
    uint32_t timer_id = 0;
    time_t cur_time = 0, session_timer = 0;

    while(true) {
        if(globals.fl_shutdown || js_ivs->fl_destroyed || !js_ivs->fl_ready) {
            break;
        }
        if(js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_SRVC_THR_DO_STOP)) {
            break;
        }

        switch_mutex_lock(js_ivs->mutex_timers);
        cur_time = switch_epoch_time_now(NULL);
        if(js_ivs->session_timeout_sec > 0) {
            if(session_timer == 0) {
                session_timer = (switch_epoch_time_now(NULL) + js_ivs->session_timeout_sec);
            } else {
                if(session_timer <= cur_time) {
                    js_ivs_event_push_session_timeout(IVS_EVENTSQ(js_ivs));
                    session_timer = (switch_epoch_time_now(NULL) + js_ivs->session_timeout_sec);
                }
            }
        }

        cur_time = switch_epoch_time_now(NULL);
        for(timer_id = 0; timer_id < IVS_TIMERS_MAX; timer_id++) {
            if(timers[timer_id].timer > 0 && timers[timer_id].interval > 0) {
                if(timers[timer_id].timer <= cur_time) {
                    js_ivs_event_push_timer_timeout(IVS_EVENTSQ(js_ivs), timer_id);
                    if(timers[timer_id].mode == IVS_TIMER_ONCE) {
                        timers[timer_id].timer = 0;
                        timers[timer_id].interval = 0;
                    } else {
                        timers[timer_id].timer = (switch_epoch_time_now(NULL) + timers[timer_id].interval);
                    }
                }
            }
        }
        switch_mutex_unlock(js_ivs->mutex_timers);

        switch_yield(10000);
    }
out:

    js_ivs_release(js_ivs);
    thread_finished();

    return NULL;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t js_ivs_service_thread_start(js_ivs_t *js_ivs) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    uint8_t fl_th_started = false;

    switch_mutex_lock(js_ivs->mutex);
    if(!js_ivs_xflags_test_unsafe(js_ivs, IVS_XFLAG_SRVC_THR_ACTIVE)) {
        js_ivs_xflags_set_unsafe(js_ivs, IVS_XFLAG_SRVC_THR_ACTIVE, true);
        status = SWITCH_STATUS_SUCCESS;
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Service thread already started\n");
        status = SWITCH_STATUS_FALSE;
    }
    switch_mutex_unlock(js_ivs->mutex);

    if(status == SWITCH_STATUS_SUCCESS) {
        if(js_ivs_take(js_ivs)) {
            launch_thread(js_ivs->pool, js_ivs_service_thread, js_ivs);
            fl_th_started = true;
        }
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(fl_th_started) {
            js_ivs_xflags_set(js_ivs, IVS_XFLAG_SRVC_THR_DO_STOP, true);
        } else {
            js_ivs_xflags_set(js_ivs, IVS_XFLAG_SRVC_THR_ACTIVE, false);
            js_ivs_release(js_ivs);
        }
    }
    return status;
}
