/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_session.h"

uint32_t js_session_take(js_session_t *session) {
    uint32_t status = false;

    if(!session) { return false; }

    switch_mutex_lock(session->mutex);
    if(session->fl_ready) {
        status = true;
        session->wlock++;
    }
    switch_mutex_unlock(session->mutex);

    return status;
}

void js_session_release(js_session_t *session) {
    switch_assert(session);

    switch_mutex_lock(session->mutex);
    if(session->wlock) { session->wlock--; }
    switch_mutex_unlock(session->mutex);
}


