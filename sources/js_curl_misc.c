/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_curl.h"

uint32_t js_curl_job_next_id(js_curl_t *js_curl) {
    uint32_t id = JID_NONE;
    if(js_curl || !js_curl->fl_destroying) {
        switch_mutex_lock(js_curl->mutex);
        id = js_curl->job_seq + 1;
        js_curl->job_seq = id;
        switch_mutex_unlock(js_curl->mutex);
    }
    return id;
}

switch_status_t js_curl_result_alloc(js_curl_result_t **result, uint32_t http_code, uint8_t *body, uint32_t body_len) {
    js_curl_result_t *result_local = NULL;

    switch_zmalloc(result_local, sizeof(js_curl_result_t));
    result_local->http_code = http_code;
    result_local->body_len = body_len;

    if(body_len > 0) {
        switch_malloc(result_local->body, body_len);
        memcpy(result_local->body, body, body_len);
    }

    *result = result_local;
    return SWITCH_STATUS_SUCCESS;
}

void js_curl_result_free(js_curl_result_t **result) {
    js_curl_result_t *res_ref = *result;
    if(res_ref) {
        switch_safe_free(res_ref->body);
        switch_safe_free(res_ref);
        *result =  NULL;
    }
}

switch_status_t js_curl_creq_conf_alloc(js_curl_creq_conf_t **conf) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    switch_memory_pool_t *pool = NULL;
    js_curl_creq_conf_t *conf_local = NULL;
    curl_conf_t *curl_conf = NULL;

    if((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }
    if((conf_local = switch_core_alloc(pool, sizeof(js_curl_creq_conf_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_core_alloc()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }
    if((status = curl_config_alloc(&curl_conf, pool, true)) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "curl_config_alloc()\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }
    conf_local->pool = pool;
    conf_local->curl_conf = curl_conf;
    conf_local->jid = JID_NONE;

    *conf = conf_local;
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(curl_conf) {
            curl_config_free(curl_conf);
        }
        if(pool) {
            switch_core_destroy_memory_pool(&pool);
        }
    }
    return status;
}

void js_curl_creq_conf_free(js_curl_creq_conf_t **conf) {
    js_curl_creq_conf_t *conf_local = *conf;
    switch_memory_pool_t *pool = (conf_local ? conf_local->pool : NULL);

    if(conf_local) {
        if(conf_local->curl_conf) {
            curl_config_free(conf_local->curl_conf);
        }
        if(pool) {
            switch_core_destroy_memory_pool(&pool);
            *conf = NULL;
        }
    }
}

switch_status_t js_curl_job_can_start(js_curl_t *js_curl) {
    if(js_curl || !js_curl->fl_destroying) {
        switch_mutex_lock(js_curl->mutex);
        js_curl->active_jobs++;
        switch_mutex_unlock(js_curl->mutex);
        return SWITCH_STATUS_SUCCESS;
    }
    return SWITCH_STATUS_FALSE;
}

void js_curl_job_finished(js_curl_t *js_curl) {
    if(!js_curl) { return; }

    switch_mutex_lock(js_curl->mutex);
    if(js_curl->active_jobs > 0) js_curl->active_jobs--;
    switch_mutex_unlock(js_curl->mutex);
}

void js_curl_queue_clean(js_curl_t *js_curl) {
    js_curl_result_t *data = NULL;

    if(js_curl && js_curl->events) {
        while(switch_queue_trypop(js_curl->events, (void *) &data) == SWITCH_STATUS_SUCCESS) {
            if(data) { js_curl_result_free(&data); }
        }
    }
}
