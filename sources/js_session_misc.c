/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

#define BIT_SET(a,b)   ((a) |= (1UL<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1UL<<(b)))
#define BIT_CHECK(a,b) (!!((a) & (1UL<<(b))))

uint32_t js_session_take(js_session_t *session) {
    uint32_t status = false;

    if(!session) { return false; }

    switch_mutex_lock(session->ivs_mutex);
    if(session->fl_ready) {
        status = true;
        session->ivs_wlock++;
    }
    switch_mutex_unlock(session->ivs_mutex);

    return status;
}

void js_session_release(js_session_t *session) {
    switch_assert(session);

    switch_mutex_lock(session->ivs_mutex);
    if(session->ivs_wlock) { session->ivs_wlock--; }
    switch_mutex_unlock(session->ivs_mutex);
}


uint32_t js_session_gen_job_id(js_session_t *session) {
    uint32_t ret = 0;

    if(!session) { return false; }

    switch_mutex_lock(session->ivs_mutex);
    if(session->ivs_jobs_seq == 0) {
        session->ivs_jobs_seq = 1;
    }
    ret = session->ivs_jobs_seq;
    session->ivs_jobs_seq++;
    switch_mutex_unlock(session->ivs_mutex);

    return ret;
}

int js_session_xflags_test(js_session_t *session, int flag) {
    switch_assert(session);
    return BIT_CHECK(session->ivs_xflags, flag);
}

void js_session_xflags_set(js_session_t *session, int flag, int val) {
    switch_assert(session);

    switch_mutex_lock(session->ivs_mutex);
    if(val) { BIT_SET(session->ivs_xflags, flag); }
    else { BIT_CLEAR(session->ivs_xflags, flag);  }
    switch_mutex_unlock(session->ivs_mutex);
}

