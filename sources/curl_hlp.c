/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include <curl_hlp.h>

extern globals_t globals;

static size_t curl_io_write_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    curl_conf_t *curl_config = (curl_conf_t *)user_data;
    size_t len = (size * nitems);

    if(len > 0) {
        switch_buffer_write(curl_config->recv_buffer, buffer, len);
    }

    return len;
}

static size_t curl_io_read_callback(char *buffer, size_t size, size_t nitems, void *user_data) {
    curl_conf_t *curl_config = (curl_conf_t *)user_data;
    size_t nmax = (size * nitems);
    size_t ncur = (curl_config->send_buffer_len > nmax) ? nmax : curl_config->send_buffer_len;

    if(ncur > 0) {
        memmove(buffer, curl_config->send_buffer_ref, ncur);
        curl_config->send_buffer_ref += ncur;
        curl_config->send_buffer_len -= ncur;
    }

    return ncur;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------
switch_status_t curl_perform(curl_conf_t *curl_config) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    CURL *curl_handle = NULL;
    curl_mime *form = NULL;
    switch_curl_slist_t *headers = NULL;
    switch_CURLcode ret_code = 0;
    long httpRes = 0;

    if(!curl_config->send_buffer_ref) {
        curl_config->send_buffer_ref = curl_config->send_buffer;
    }

    curl_handle = switch_curl_easy_init();
    headers = switch_curl_slist_append(headers, curl_config->content_type);

    switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);

    if(curl_config->method == CURL_METHOD_GET) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1);
    } else if(curl_config->method == CURL_METHOD_POST) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
    } else if(curl_config->method == CURL_METHOD_PUT) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_UPLOAD, 1);
        switch_curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if(curl_config->method == CURL_METHOD_DELETE) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    if(curl_config->send_buffer) {
        if(curl_config->method == CURL_METHOD_POST || curl_config->method == CURL_METHOD_DELETE || curl_config->method == CURL_METHOD_PUT) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, curl_config->send_buffer_len);
            switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (void *) curl_config->send_buffer_ref);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_READFUNCTION, curl_io_read_callback);
        switch_curl_easy_setopt(curl_handle, CURLOPT_READDATA, (void *) curl_config);
    }
    if(curl_config->recv_buffer) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, curl_io_write_callback);
        switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) curl_config);
    }

    if(curl_config->connect_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, curl_config->connect_timeout);
    }
    if(curl_config->request_timeout > 0) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, curl_config->request_timeout);
    }
    if(curl_config->user_agent) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, curl_config->user_agent);
    }
    if(curl_config->credentials) {
        if(curl_config->auth_type == CURLAUTH_BEARER) { // require curl-7.61.0 or higher
            curl_easy_setopt(curl_handle, CURLOPT_XOAUTH2_BEARER, curl_config->credentials);
            curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BEARER);
        } else {
            switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, curl_config->auth_type);
            switch_curl_easy_setopt(curl_handle, CURLOPT_USERPWD, curl_config->credentials);
        }
    }
    if(strncasecmp(curl_config->url, "https", 5) == 0) {
        if(!zstr(curl_config->cacert)) {
             switch_curl_easy_setopt(curl_handle, CURLOPT_CAINFO, curl_config->cacert);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, curl_config->ssl_verfypeer);
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, curl_config->ssl_verfyhost);
    }
    if(curl_config->proxy) {
        if(curl_config->proxy_credentials != NULL) {
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXYUSERPWD, curl_config->proxy_credentials);
        }
        if(strncasecmp(curl_config->proxy, "https", 5) == 0) {
            if(!zstr(curl_config->proxy_cacert)) {
                switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY_CAINFO, curl_config->proxy_cacert);
            }
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY_SSL_VERIFYPEER, curl_config->proxy_insecure);
            switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY_SSL_VERIFYHOST, curl_config->proxy_insecure);
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_PROXY, curl_config->proxy);
    }

    if(curl_config->fields) {
        form = curl_mime_init(curl_handle);
        xcurl_from_field_t *field_conf = curl_config->fields;

        while(field_conf) {
            curl_mimepart *field = curl_mime_addpart(form);
            if(field) {
                curl_mime_name(field, field_conf->name);
                if(field_conf->type == CURL_FIELD_TYPE_SIMPLE)  {
                    if(field_conf->value) { curl_mime_data(field, field_conf->value, CURL_ZERO_TERMINATED); }
                } else if(field_conf->type == CURL_FIELD_TYPE_FILE)  {
                    if(field_conf->value) { curl_mime_filedata(field, field_conf->value); }
                }
            }
            field_conf = (xcurl_from_field_t *)field_conf->next;
        }
        switch_curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, form);
    }

    headers = switch_curl_slist_append(headers, "Expect:");
    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, curl_config->url);

    ret_code = switch_curl_easy_perform(curl_handle);
    if(!ret_code) { switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &httpRes); }
    else { httpRes = ret_code; }

    curl_config->http_error = httpRes;
    if(httpRes != 200) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "http-error=[%ld] (%s)\n", httpRes, curl_config->url);
        status = SWITCH_STATUS_FALSE;
    }

    if(curl_handle) {
        switch_curl_easy_cleanup(curl_handle);
    }
    if(form) {
        curl_mime_free(form);
    }
    if(headers) {
        switch_curl_slist_free_all(headers);
    }

    return status;
}

switch_status_t curl_field_add(curl_conf_t *curl_config, int type, char *name, char *value) {
    xcurl_from_field_t *field_conf = NULL;

    field_conf = switch_core_alloc(curl_config->pool, sizeof(xcurl_from_field_t));
    if(field_conf == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mem fail\n");
        return SWITCH_STATUS_FALSE;
    }

    field_conf->type = type;
    field_conf->name = (name ? switch_core_strdup(curl_config->pool, name) : NULL);
    field_conf->value = (value ? switch_core_strdup(curl_config->pool, value) : NULL);

    if(curl_config->fields == NULL) {
        curl_config->fields = field_conf;
        curl_config->fields_tail = field_conf;
    } else {
       curl_config->fields_tail->next = (xcurl_from_field_t *)field_conf;
       curl_config->fields_tail = field_conf;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t curl_config_alloc(curl_conf_t **curl_config, switch_memory_pool_t *pool, uint8_t with_recvbuff) {
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    switch_memory_pool_t *lpool = pool;
    curl_conf_t *lconf = NULL;

    if(pool == NULL) {
        if((status = switch_core_new_memory_pool(&lpool)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pool fail\n");
            switch_goto_status(SWITCH_STATUS_GENERR, out);
        }
    }

    if((lconf = switch_core_alloc(lpool, sizeof(curl_conf_t))) == NULL) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mem fail\n");
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if(with_recvbuff) {
        if(switch_buffer_create_dynamic(&lconf->recv_buffer, 1024, 4096, 16384) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail (recv_buffer)\n");
            switch_goto_status(SWITCH_STATUS_GENERR, out);
        }
    } else {
        lconf->recv_buffer = NULL;
    }

    lconf->fl_ext_pool = (pool == NULL ? false : true);
    lconf->pool = lpool;
    lconf->fields = NULL;
    lconf->send_buffer = NULL;
    lconf->send_buffer_len = 0;
    lconf->auth_type = CURLAUTH_ANY;
    lconf->method = CURL_METHOD_GET;
    lconf->ssl_verfypeer = 0;
    lconf->ssl_verfyhost = 0;

    *curl_config = lconf;
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        if(lconf) {
            if(lconf->recv_buffer) {
                switch_buffer_destroy(&lconf->recv_buffer);
            }
        }
        if(lpool != NULL && pool == NULL) {
            switch_core_destroy_memory_pool(&lpool);
        }
    }
    return status;
}

void curl_config_free(curl_conf_t *curl_config) {
    switch_memory_pool_t *pool = (curl_config ? curl_config->pool : NULL);

    if(curl_config) {
        if(curl_config->recv_buffer) {
            switch_buffer_destroy(&curl_config->recv_buffer);
        }
        if(curl_config->fl_ext_pool == false) {
            if(pool) {
                switch_core_destroy_memory_pool(&pool);
            }
        }
    }
}

uint32_t curl_method2id(const char *name) {
    if(zstr(name)) {
        return CURL_METHOD_GET;
    }
    if(strcasecmp(name, "GET") == 0) {
        return CURL_METHOD_GET;
    }
    if(strcasecmp(name, "POST") == 0) {
        return CURL_METHOD_POST;
    }
    if(strcasecmp(name, "PUT") == 0) {
        return CURL_METHOD_PUT;
    }
    if(strcasecmp(name, "DELETE") == 0) {
        return CURL_METHOD_DELETE;
    }
    return CURL_METHOD_GET;
}

const char *curl_method2name(uint32_t id) {
    switch(id) {
        case CURL_METHOD_GET    : return "GET";
        case CURL_METHOD_POST   : return "POST";
        case CURL_METHOD_PUT    : return "PUT";
        case CURL_METHOD_DELETE : return "DELETE";
    }
    return "GET";
}
