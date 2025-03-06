/**
 * (C)2023 aks
 * https://github.com/akscf/
 **/
#include "js_curl.h"

#define CLASS_NAME              "CURL"
#define PROP_URL                1
#define PROP_PROXY              2
#define PROP_METHOD             3
#define PROP_REQ_TIMEOUT        4
#define PROP_CONN_TIMEOUT       5
#define PROP_USER_AGENT         6
#define PROP_SSL_VERFYPEER      7
#define PROP_SSL_VERFYHOST      8
#define PROP_SSL_CACERT         9
#define PROP_SSL_PROXY_CACERT   10
#define PROP_CREDENTIALS        11
#define PROP_PROXY_CREDENTIALS  12
#define PROP_CONTENT_TYPE       13
#define PROP_AUTH_TYPE          14

#define DEFAULT_CONTENT_TYPE    "text/plain"

static void js_curl_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static js_curl_result_t *js_curl_request_exec(js_curl_creq_conf_t *creq_conf) {
    switch_status_t status = SWITCH_STATUS_FALSE;
    curl_conf_t *curl_conf = creq_conf->curl_conf;
    js_curl_result_t *result = NULL;
    const void *http_response_ptr = NULL;
    uint32_t recv_len = 0;

    status = curl_perform(curl_conf);

    recv_len = switch_buffer_inuse(curl_conf->recv_buffer);
    if(recv_len > 0) {
        switch_buffer_peek_zerocopy(curl_conf->recv_buffer, &http_response_ptr);
        js_curl_result_alloc(&result, curl_conf->http_error, (char *)http_response_ptr, recv_len);
    } else {
        js_curl_result_alloc(&result, curl_conf->http_error, NULL, 0);
    }

    return result;
}

static void *SWITCH_THREAD_FUNC js_curl_request_exec_thread(switch_thread_t *thread, void *obj) {
    volatile js_curl_creq_conf_t *_ref = (js_curl_creq_conf_t *) obj;
    js_curl_creq_conf_t *creq_conf = (js_curl_creq_conf_t *) _ref;
    js_curl_t *js_curl = creq_conf->js_curl_ref;
    js_curl_result_t *res = NULL;

    res = js_curl_request_exec(creq_conf);
    if(res) {
        res->jid = creq_conf->jid;
        if(switch_queue_trypush(js_curl->events, res) != SWITCH_STATUS_SUCCESS) {
            js_curl_result_free(&res);
        }
    }

    js_curl_creq_conf_free(&creq_conf);

    js_curl_job_finished(js_curl);
    thread_finished();

    return NULL;
}

static uint32_t js_curl_request_exec_async(js_curl_creq_conf_t *creq_conf) {
    uint32_t jid = JID_NONE;

    if(js_curl_job_can_start(creq_conf->js_curl_ref) == SWITCH_STATUS_SUCCESS) {
        jid = creq_conf->jid = js_curl_job_next_id(creq_conf->js_curl_ref);
        launch_thread(creq_conf->pool, js_curl_request_exec_thread, creq_conf);
    }

    return jid;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_curl_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));

    if(!js_curl) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_URL: {
            return JS_NewString(ctx, js_curl->url);
        }
        case PROP_METHOD: {
            return JS_NewString(ctx, curl_method2name(js_curl->method));
        }
        case PROP_SSL_VERFYPEER: {
            return(js_curl->fl_ssl_verfypeer ? JS_TRUE : JS_FALSE);
        }
        case PROP_SSL_VERFYHOST: {
            return(js_curl->fl_ssl_verfyhost ? JS_TRUE : JS_FALSE);
        }
        case PROP_PROXY: {
            if(!zstr(js_curl->proxy)) {
               return JS_NewString(ctx, js_curl->proxy);
            }
            return JS_UNDEFINED;
        }
        case PROP_SSL_CACERT: {
            if(!zstr(js_curl->cacert)) {
               return JS_NewString(ctx, js_curl->cacert);
            }
            return JS_UNDEFINED;
        }
        case PROP_SSL_PROXY_CACERT: {
            if(!zstr(js_curl->proxy_cacert)) {
               return JS_NewString(ctx, js_curl->proxy_cacert);
            }
            return JS_UNDEFINED;
        }
        case PROP_CREDENTIALS: {
            if(!zstr(js_curl->credentials)) {
               return JS_NewString(ctx, js_curl->credentials);
            }
            return JS_UNDEFINED;
        }
        case PROP_PROXY_CREDENTIALS: {
            if(!zstr(js_curl->proxy_credentials)) {
               return JS_NewString(ctx, js_curl->proxy_credentials);
            }
            return JS_UNDEFINED;
        }
        case PROP_CONTENT_TYPE: {
            if(!zstr(js_curl->content_type)) {
                return JS_NewString(ctx, js_curl->content_type);
            }
            return JS_UNDEFINED;
        }
        case PROP_USER_AGENT: {
            if(!zstr(js_curl->user_agent)) {
               return JS_NewString(ctx, js_curl->user_agent);
            }
            return JS_UNDEFINED;
        }
        case PROP_CONN_TIMEOUT: {
            return JS_NewInt32(ctx, js_curl->connect_timeout);
        }
        case PROP_REQ_TIMEOUT: {
            return JS_NewInt32(ctx, js_curl->request_timeout);
        }
        case PROP_AUTH_TYPE: {
            switch(js_curl->auth_type) {
                case CURLAUTH_NONE    : return JS_NewString(ctx, "none");
                case CURLAUTH_BASIC   : return JS_NewString(ctx, "basic");
                case CURLAUTH_DIGEST  : return JS_NewString(ctx, "digest");
                case CURLAUTH_BEARER  : return JS_NewString(ctx, "bearer");
                case CURLAUTH_ANY     : return JS_NewString(ctx, "any");
            }
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_curl_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    const char *str = NULL;
    int copy = 1, success = 1;


    if(!js_curl) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_URL: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            str = JS_ToCString(ctx, val);
            if(!zstr(js_curl->url)) { copy = strcmp(js_curl->url, str); }
            if(copy) { js_curl->url = switch_core_strdup(js_curl->pool, str); }
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_PROXY: {
            if(QJS_IS_NULL(val)) {
                js_curl->proxy = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_curl->proxy)) { copy = strcmp(js_curl->proxy, str); }
                if(copy) { js_curl->proxy = switch_core_strdup(js_curl->pool, str); }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
        case PROP_SSL_CACERT: {
            if(QJS_IS_NULL(val)) {
                js_curl->cacert = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(switch_file_exists(str, js_curl->pool) != SWITCH_STATUS_SUCCESS) {
                    char *tfile = NULL;
                    tfile = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, str);

                    if(switch_file_exists(tfile, js_curl->pool) != SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File not found: %s\n", tfile);
                        success = 0;
                    } else {
                        if(!zstr(js_curl->cacert)) { copy = strcmp(js_curl->cacert, tfile); }
                        if(copy) { js_curl->cacert = switch_core_strdup(js_curl->pool, tfile); }
                    }

                    switch_safe_free(tfile);
                } else {
                    if(!zstr(js_curl->cacert)) { copy = strcmp(js_curl->cacert, str); }
                    if(copy) { js_curl->cacert = switch_core_strdup(js_curl->pool, str); }
                }
                JS_FreeCString(ctx, str);
            }
            return (success ? JS_TRUE : JS_FALSE);
        }
        case PROP_SSL_PROXY_CACERT: {
            if(QJS_IS_NULL(val)) {
                js_curl->proxy_cacert = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(switch_file_exists(str, js_curl->pool) != SWITCH_STATUS_SUCCESS) {
                    char *tfile = NULL;
                    tfile = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, str);

                    if(switch_file_exists(tfile, js_curl->pool) != SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "File not found: %s\n", tfile);
                        success = 0;
                    } else {
                        if(!zstr(js_curl->proxy_cacert)) { copy = strcmp(js_curl->proxy_cacert, tfile); }
                        if(copy) { js_curl->proxy_cacert = switch_core_strdup(js_curl->pool, tfile); }
                    }

                    switch_safe_free(tfile);
                } else {
                    if(!zstr(js_curl->proxy_cacert)) { copy = strcmp(js_curl->proxy_cacert, str); }
                    if(copy) { js_curl->proxy_cacert = switch_core_strdup(js_curl->pool, str); }
                }
                JS_FreeCString(ctx, str);
            }
            return (success ? JS_TRUE : JS_FALSE);
        }
        case PROP_METHOD: {
            if(QJS_IS_NULL(val)) { return JS_FALSE; }
            str = JS_ToCString(ctx, val);
            js_curl->method = curl_method2id(str);
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_CREDENTIALS: {
            if(QJS_IS_NULL(val)) {
                js_curl->credentials = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_curl->credentials)) { copy = strcmp(js_curl->credentials, str); }
                if(copy) { js_curl->credentials = switch_core_strdup(js_curl->pool, str); }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
        case PROP_PROXY_CREDENTIALS: {
            if(QJS_IS_NULL(val)) {
                js_curl->proxy_credentials = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_curl->proxy_credentials)) { copy = strcmp(js_curl->proxy_credentials, str); }
                if(copy) { js_curl->proxy_credentials = switch_core_strdup(js_curl->pool, str); }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
        case PROP_CONTENT_TYPE: {
            if(QJS_IS_NULL(val)) {
                js_curl->content_type = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_curl->content_type)) { copy = strcmp(js_curl->content_type, str); }
                if(copy) { js_curl->content_type = switch_core_strdup(js_curl->pool, str); }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
        case PROP_USER_AGENT: {
            if(QJS_IS_NULL(val)) {
                js_curl->user_agent = NULL;
            } else {
                str = JS_ToCString(ctx, val);
                if(!zstr(js_curl->user_agent)) { copy = strcmp(js_curl->user_agent, str); }
                if(copy) { js_curl->user_agent = switch_core_strdup(js_curl->pool, str); }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
        case PROP_REQ_TIMEOUT: {
            JS_ToUint32(ctx, &js_curl->request_timeout, val);
            return JS_TRUE;
        }
        case PROP_CONN_TIMEOUT: {
            JS_ToUint32(ctx, &js_curl->connect_timeout, val);
            return JS_TRUE;
        }
        case PROP_SSL_VERFYPEER: {
            js_curl->fl_ssl_verfypeer = JS_ToBool(ctx, val);
            return JS_TRUE;
        }
        case PROP_SSL_VERFYHOST: {
            js_curl->fl_ssl_verfyhost = JS_ToBool(ctx, val);
            return JS_TRUE;
        }
        case PROP_AUTH_TYPE: {
            if(QJS_IS_NULL(val)) {
                js_curl->auth_type = CURLAUTH_NONE;
            } else {
                str = JS_ToCString(ctx, val);
                if(strcasecmp(str, "none") == 0) {
                    js_curl->auth_type = CURLAUTH_NONE;
                } else if(strcasecmp(str, "basic") == 0) {
                    js_curl->auth_type = CURLAUTH_BASIC;
                } else if(strcasecmp(str, "digest") == 0) {
                    js_curl->auth_type = CURLAUTH_DIGEST;
                } else if(strcasecmp(str, "bearer") == 0) {
                    js_curl->auth_type = CURLAUTH_BEARER;
                } else if(strcasecmp(str, "any") == 0) {
                    js_curl->auth_type = CURLAUTH_ANY;
                }
                JS_FreeCString(ctx, str);
            }
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

/**
 ** perform( [string|arrayBuffer] || {type: [file|simple], name: fieldName, value: fieldValue}, {...})
 **/
static JSValue js_curl_perform_request(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    js_curl_creq_conf_t *creq_conf = NULL;
    JSValue ret_obj = JS_FALSE;

    if(!js_curl || js_curl->fl_destroying) {
        return JS_ThrowTypeError(ctx, "Context destroyed");
    }

    if(zstr(js_curl->url)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "js_curl->url == NULL\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    if((status = js_curl_creq_conf_alloc(&creq_conf)) != SWITCH_STATUS_SUCCESS) {
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if(argc > 0) {
        for(int i = 0; i < argc; i++) {
            if(JS_IsString(argv[i])) {
                if(creq_conf->curl_conf->send_buffer == NULL) {
                    const char *data = JS_ToCString(ctx, argv[i]);
                    if(data) {
                        creq_conf->curl_conf->send_buffer = switch_core_strdup(creq_conf->pool, data);
                        creq_conf->curl_conf->send_buffer_len = strlen(creq_conf->curl_conf->send_buffer);
                    }
                    JS_FreeCString(ctx, data);
                }
            } else if(JS_IsObject(argv[i])) {
                switch_size_t abuf_len = 0;
                uint8_t *abuf = NULL;

                abuf = JS_GetArrayBuffer(ctx, &abuf_len, argv[i]);
                if(abuf && abuf_len > 0)  {
                    if(creq_conf->curl_conf->send_buffer == NULL) {
                        creq_conf->curl_conf->send_buffer = safe_pool_bufdup(creq_conf->pool, abuf, abuf_len);
                        creq_conf->curl_conf->send_buffer_len = abuf_len;
                    }
                } else {
                    JSValue field_type,  field_name, field_value;
                    field_type = JS_GetPropertyStr(ctx, argv[i], "type");
                    field_name = JS_GetPropertyStr(ctx, argv[i], "name");
                    field_value = JS_GetPropertyStr(ctx, argv[i], "value");

                    if(JS_IsString(field_type) && JS_IsString(field_name)) {
                        const char *ftype = NULL, *fname = NULL, *fval = NULL;
                        ftype = JS_ToCString(ctx, field_type);
                        fname = JS_ToCString(ctx, field_name);
                        fval = JS_ToCString(ctx, field_value);

                        status = curl_field_add(creq_conf->curl_conf, (!strcasecmp(ftype, "file") ? CURL_FIELD_TYPE_FILE : CURL_FIELD_TYPE_SIMPLE), (char *)fname, (char *)fval);

                        JS_FreeCString(ctx, ftype);
                        JS_FreeCString(ctx, fname);
                        JS_FreeCString(ctx, fval);

                        if(status != SWITCH_STATUS_SUCCESS) { break; }
                    }
                }
            }
        }
    }

    if(status == SWITCH_STATUS_SUCCESS) {
        creq_conf->js_curl_ref = js_curl;
        creq_conf->curl_conf->content_type = switch_core_sprintf(creq_conf->pool, "Content-Type: %s", (js_curl->content_type ? js_curl->content_type : DEFAULT_CONTENT_TYPE));
        creq_conf->curl_conf->url = safe_pool_strdup(creq_conf->pool, js_curl->url);
        creq_conf->curl_conf->user_agent = safe_pool_strdup(creq_conf->pool, js_curl->user_agent);
        creq_conf->curl_conf->credentials = safe_pool_strdup(creq_conf->pool, js_curl->credentials);
        creq_conf->curl_conf->proxy_credentials = safe_pool_strdup(creq_conf->pool, js_curl->proxy_credentials);
        creq_conf->curl_conf->proxy = safe_pool_strdup(creq_conf->pool, js_curl->proxy);
        creq_conf->curl_conf->cacert = safe_pool_strdup(creq_conf->pool, js_curl->cacert);
        creq_conf->curl_conf->proxy_cacert = safe_pool_strdup(creq_conf->pool, js_curl->proxy_cacert);
        creq_conf->curl_conf->request_timeout = js_curl->request_timeout;
        creq_conf->curl_conf->connect_timeout = js_curl->connect_timeout;
        creq_conf->curl_conf->method = js_curl->method;
        creq_conf->curl_conf->auth_type = js_curl->auth_type;
        creq_conf->curl_conf->ssl_verfyhost = js_curl->fl_ssl_verfyhost;
        creq_conf->curl_conf->ssl_verfypeer = js_curl->fl_ssl_verfypeer;

        js_curl_result_t *res = js_curl_request_exec(creq_conf);
        if(res) {
            ret_obj = JS_NewObject(ctx);

            JS_SetPropertyStr(ctx, ret_obj, "body", (res->body_len > 0 ? JS_NewStringLen(ctx, res->body, res->body_len) : JS_UNDEFINED));
            JS_SetPropertyStr(ctx, ret_obj, "code", JS_NewInt32(ctx, res->http_code));

            js_curl_result_free(&res);
        }
    }
out:
    js_curl_creq_conf_free(&creq_conf);
    return ret_obj;
}

/**
 ** bgPerform( [string|arrayBuffer] || {type: [file|simple], name: fieldName, value: fieldValue}, {...})
 **/
static JSValue js_curl_perform_bg_request(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    js_curl_creq_conf_t *creq_conf = NULL;
    JSValue ret_obj = JS_FALSE;

    if(!js_curl || js_curl->fl_destroying) {
        return JS_ThrowTypeError(ctx, "Context destroyed");
    }

    if(zstr(js_curl->url)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "js_curl->url == NULL\n");
        switch_goto_status(SWITCH_STATUS_FALSE, out);
    }

    if((status = js_curl_creq_conf_alloc(&creq_conf)) != SWITCH_STATUS_SUCCESS) {
        switch_goto_status(SWITCH_STATUS_GENERR, out);
    }

    if(argc > 0) {
        for(int i = 0; i < argc; i++) {
            if(JS_IsString(argv[i])) {
                if(creq_conf->curl_conf->send_buffer == NULL) {
                    const char *data = JS_ToCString(ctx, argv[i]);
                    if(data) {
                        creq_conf->curl_conf->send_buffer = switch_core_strdup(creq_conf->pool, data);
                        creq_conf->curl_conf->send_buffer_len = strlen(creq_conf->curl_conf->send_buffer);
                    }
                    JS_FreeCString(ctx, data);
                }
            } else if(JS_IsObject(argv[i])) {
                switch_size_t abuf_len = 0;
                uint8_t *abuf = NULL;

                abuf = JS_GetArrayBuffer(ctx, &abuf_len, argv[i]);
                if(abuf && abuf_len > 0)  {
                    if(creq_conf->curl_conf->send_buffer == NULL) {
                        creq_conf->curl_conf->send_buffer = safe_pool_bufdup(creq_conf->pool, abuf, abuf_len);
                        creq_conf->curl_conf->send_buffer_len = abuf_len;
                    }
                } else {
                    JSValue field_type,  field_name, field_value;
                    field_type = JS_GetPropertyStr(ctx, argv[i], "type");
                    field_name = JS_GetPropertyStr(ctx, argv[i], "name");
                    field_value = JS_GetPropertyStr(ctx, argv[i], "value");

                    if(JS_IsString(field_type) && JS_IsString(field_name)) {
                        const char *ftype = NULL, *fname = NULL, *fval = NULL;
                        ftype = JS_ToCString(ctx, field_type);
                        fname = JS_ToCString(ctx, field_name);
                        fval = JS_ToCString(ctx, field_value);

                        status = curl_field_add(creq_conf->curl_conf, (!strcasecmp(ftype, "file") ? CURL_FIELD_TYPE_FILE : CURL_FIELD_TYPE_SIMPLE), (char *)fname, (char *)fval);

                        JS_FreeCString(ctx, ftype);
                        JS_FreeCString(ctx, fname);
                        JS_FreeCString(ctx, fval);

                        if(status != SWITCH_STATUS_SUCCESS) { break; }
                    }
                }
            }
        }
    }

    if(status == SWITCH_STATUS_SUCCESS) {
        creq_conf->js_curl_ref = js_curl;
        creq_conf->curl_conf->content_type = switch_core_sprintf(creq_conf->pool, "Content-Type: %s", (js_curl->content_type ? js_curl->content_type : DEFAULT_CONTENT_TYPE));
        creq_conf->curl_conf->url = safe_pool_strdup(creq_conf->pool, js_curl->url);
        creq_conf->curl_conf->user_agent = safe_pool_strdup(creq_conf->pool, js_curl->user_agent);
        creq_conf->curl_conf->credentials = safe_pool_strdup(creq_conf->pool, js_curl->credentials);
        creq_conf->curl_conf->proxy_credentials = safe_pool_strdup(creq_conf->pool, js_curl->proxy_credentials);
        creq_conf->curl_conf->proxy = safe_pool_strdup(creq_conf->pool, js_curl->proxy);
        creq_conf->curl_conf->cacert = safe_pool_strdup(creq_conf->pool, js_curl->cacert);
        creq_conf->curl_conf->proxy_cacert = safe_pool_strdup(creq_conf->pool, js_curl->proxy_cacert);
        creq_conf->curl_conf->connect_timeout = js_curl->connect_timeout;
        creq_conf->curl_conf->request_timeout = js_curl->request_timeout;
        creq_conf->curl_conf->method = js_curl->method;
        creq_conf->curl_conf->auth_type = js_curl->auth_type;
        creq_conf->curl_conf->ssl_verfyhost = js_curl->fl_ssl_verfyhost;
        creq_conf->curl_conf->ssl_verfypeer = js_curl->fl_ssl_verfypeer;

        uint32_t jid = js_curl_request_exec_async(creq_conf);
        ret_obj = (jid > 0 ? JS_NewInt32(ctx, jid) : JS_FALSE);
    }
out:
    if(status != SWITCH_STATUS_SUCCESS) {
        js_curl_creq_conf_free(&creq_conf);
    }
    return ret_obj;
}

static JSValue js_curl_get_bg_request_result(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    switch_status_t status = SWITCH_STATUS_SUCCESS;
    js_curl_creq_conf_t *creq_conf = NULL;
    JSValue ret_obj = JS_UNDEFINED;
    void *pop = NULL;

    if(!js_curl || js_curl->fl_destroying) {
        return JS_ThrowTypeError(ctx, "Context destroyed");
    }

    if(switch_queue_trypop(js_curl->events, &pop) == SWITCH_STATUS_SUCCESS) {
        js_curl_result_t *cresult = (js_curl_result_t *)pop;
        if(cresult) {
            ret_obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, ret_obj, "class",JS_NewString(ctx, "CurlResult"));
            JS_SetPropertyStr(ctx, ret_obj, "jid",  JS_NewInt32(ctx, cresult->jid));
            JS_SetPropertyStr(ctx, ret_obj, "body", JS_NewStringLen(ctx, cresult->body, cresult->body_len));
            JS_SetPropertyStr(ctx, ret_obj, "code", JS_NewInt32(ctx, cresult->http_code));
        }
        js_curl_result_free(&cresult);
    }

    return ret_obj;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_curl_class = {
    CLASS_NAME,
    .finalizer = js_curl_finalizer,
};

static const JSCFunctionListEntry js_curl_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("url", js_curl_property_get, js_curl_property_set, PROP_URL),
    JS_CGETSET_MAGIC_DEF("method", js_curl_property_get, js_curl_property_set, PROP_METHOD),
    JS_CGETSET_MAGIC_DEF("requestTimeout", js_curl_property_get, js_curl_property_set, PROP_REQ_TIMEOUT),
    JS_CGETSET_MAGIC_DEF("connectTimeout", js_curl_property_get, js_curl_property_set, PROP_CONN_TIMEOUT),
    JS_CGETSET_MAGIC_DEF("authType", js_curl_property_get, js_curl_property_set, PROP_AUTH_TYPE),
    JS_CGETSET_MAGIC_DEF("userAgent", js_curl_property_get, js_curl_property_set, PROP_USER_AGENT),
    JS_CGETSET_MAGIC_DEF("credentials", js_curl_property_get, js_curl_property_set, PROP_CREDENTIALS),
    JS_CGETSET_MAGIC_DEF("contentType", js_curl_property_get, js_curl_property_set, PROP_CONTENT_TYPE),
    JS_CGETSET_MAGIC_DEF("sslVerfyPeer", js_curl_property_get, js_curl_property_set, PROP_SSL_VERFYPEER),
    JS_CGETSET_MAGIC_DEF("sslVerfyHost", js_curl_property_get, js_curl_property_set, PROP_SSL_VERFYHOST),
    JS_CGETSET_MAGIC_DEF("sslCAcert", js_curl_property_get, js_curl_property_set, PROP_SSL_CACERT),
    JS_CGETSET_MAGIC_DEF("proxy", js_curl_property_get, js_curl_property_set, PROP_PROXY),
    JS_CGETSET_MAGIC_DEF("proxyCredentials", js_curl_property_get, js_curl_property_set, PROP_PROXY_CREDENTIALS),
    JS_CGETSET_MAGIC_DEF("proxyCAcert", js_curl_property_get, js_curl_property_set, PROP_SSL_PROXY_CACERT),
    //
    JS_CFUNC_DEF("perform", 1, js_curl_perform_request),
    JS_CFUNC_DEF("performBg", 1, js_curl_perform_bg_request),
    JS_CFUNC_DEF("getResult", 1, js_curl_get_bg_request_result),
};

static void js_curl_finalizer(JSRuntime *rt, JSValue val) {
    js_curl_t *js_curl = JS_GetOpaque(val, js_curl_get_classid2(rt));
    switch_memory_pool_t *pool = (js_curl ? js_curl->pool : NULL);
    uint8_t fl_wloop = true;

    if(!js_curl || js_curl->fl_destroying) {
        return;
    }

    js_curl->fl_destroying = true;

    if(js_curl->events) {
        js_curl_queue_clean(js_curl);
        switch_queue_term(js_curl->events);
    }

    switch_mutex_lock(js_curl->mutex);
    fl_wloop = (js_curl->active_jobs != 0);
    switch_mutex_unlock(js_curl->mutex);

    if(fl_wloop) {
#ifdef MOD_QUICKJS_DEBUG
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for termination of '%d' jobs...\n", js_curl->active_jobs);
#endif
        while(fl_wloop) {
            switch_mutex_lock(js_curl->mutex);
            fl_wloop = (js_curl->active_jobs != 0);
            switch_mutex_unlock(js_curl->mutex);
            switch_yield(100000);
        }
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-curl-finalizer: js_curl=%p (destroyed)\n", js_curl);
#endif

    js_free_rt(rt, js_curl);
}

/*
 * new CURL(url, [method, timeout, credentials, contentType])
*/
static JSValue js_curl_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_curl_t *js_curl = NULL;
    switch_memory_pool_t *pool = NULL;
    const char *content_type = NULL;
    const char *credentials = NULL;
    const char *method = NULL;
    const char *url = NULL;
    uint32_t timeout = 0;

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "CURL(url, [method, timeout, credentials, contentType])");
    }

    url = JS_ToCString(ctx, argv[0]);

    if(argc > 1 && !QJS_IS_NULL(argv[1])) {
        method = JS_ToCString(ctx, argv[1]);
    }

    if(argc > 2) {
        JS_ToUint32(ctx, &timeout, argv[2]);
    }

    if(argc > 3 && !QJS_IS_NULL(argv[3])) {
        credentials = JS_ToCString(ctx, argv[3]);
    }

    if(argc > 4 && !QJS_IS_NULL(argv[4])) {
        content_type = JS_ToCString(ctx, argv[4]);
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto fail;
    }

    js_curl = js_mallocz(ctx, sizeof(js_curl_t));
    if(!js_curl) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        goto fail;
    }

    js_curl->pool = pool;
    js_curl->url = switch_core_strdup(pool, url);
    js_curl->method = curl_method2id(method);
    js_curl->credentials = (credentials ? switch_core_strdup(pool, credentials) : NULL);
    js_curl->content_type = (content_type ? switch_core_strdup(pool, content_type) : DEFAULT_CONTENT_TYPE);
    js_curl->request_timeout = timeout;
    js_curl->connect_timeout = timeout;
    js_curl->fl_destroying = false;

    switch_queue_create(&js_curl->events, CURL_QUEUE_SIZE, pool);
    if(switch_mutex_init(&js_curl->mutex, SWITCH_MUTEX_NESTED, pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_mutex_init()\n");
        goto fail;
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_curl_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_curl);

    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, method);
    JS_FreeCString(ctx, credentials);
    JS_FreeCString(ctx, content_type);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-curl-constructor: js_curl=%p\n", js_curl);
#endif

    return obj;
fail:
    if(js_curl) {
        if(js_curl->events) {
            switch_queue_term(js_curl->events);
        }
        js_free(ctx, js_curl);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, method);
    JS_FreeCString(ctx, credentials);
    JS_FreeCString(ctx, content_type);

    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_curl_get_classid2(JSRuntime *rt) {
    script_t *script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);
    return script->class_id_curl;
}
JSClassID js_curl_get_classid(JSContext *ctx) {
    return  js_curl_get_classid2(JS_GetRuntime(ctx));
}

switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id) {
    JSValue obj_proto, obj_class;
    script_t *script = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    switch_assert(script);

    if(JS_IsRegisteredClass(JS_GetRuntime(ctx), class_id)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Class with id (%d) already registered!\n", class_id);
        return SWITCH_STATUS_FALSE;
    }

    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, &js_curl_class);
    script->class_id_curl = class_id;

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Class registered [%s / %d]\n", CLASS_NAME, class_id);
#endif

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_curl_proto_funcs, ARRAY_SIZE(js_curl_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_curl_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

