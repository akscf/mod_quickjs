/**
 * Curl
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.org/
 **/

#include "mod_quickjs.h"
#ifdef JS_CURL_ENABLE

#define CLASS_NAME          "CURL"
#define PROP_IS_READY       0
#define PROP_URL            1
#define PROP_METHOD         2
#define PROP_TIMEOUT        3
#define PROP_CREDENTIALS    4
#define PROP_CONTENT_TYPE   5

// application/x-www-form-urlencoded
#define DEFAULT_CONTENT_TYPE    "application/json"

#define CURL_SANITY_CHECK() if (!js_curl) { \
           return JS_ThrowTypeError(ctx, "CURL is not initialized"); \
        }

static void js_curl_finalizer(JSRuntime *rt, JSValue val);
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *data);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_curl_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));

    if(magic == PROP_IS_READY) {
        uint8_t x = (js_curl != NULL);
        return (x ? JS_TRUE : JS_FALSE);
    }

    if(!js_curl) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_URL: {
            return JS_NewString(ctx, js_curl->url);
        }
        case PROP_METHOD: {
            return JS_NewString(ctx, js_curl->method);
        }
        case PROP_CREDENTIALS: {
            if(!zstr(js_curl->credentials)) {
               return JS_NewString(ctx, js_curl->credentials);
            }
            return JS_UNDEFINED;
        }
        case PROP_CONTENT_TYPE: {
            if(!zstr(js_curl->content_type)) {
                return JS_NewString(ctx, js_curl->content_type);
            }
            return JS_UNDEFINED;
        }
        case PROP_TIMEOUT: {
            return JS_NewInt32(ctx, js_curl->timeout);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_curl_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    const char *str = NULL;
    int copy = 1;

    if(!js_curl) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_URL: {
            str = JS_ToCString(ctx, val);
            if(zstr(str)) { return JS_FALSE; }
            if(strcmp(js_curl->url, str)) {
                js_curl->url = switch_core_strdup(js_curl->pool, str);
            }
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_METHOD: {
            str = JS_ToCString(ctx, val);
            if(zstr(str)) { return JS_FALSE; }
            if(strcmp(js_curl->method, str)) {
                js_curl->method = switch_core_strdup(js_curl->pool, str);
            }
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_CREDENTIALS: {
            str = JS_ToCString(ctx, val);
            if(zstr(str)) {
                js_curl->credentials = NULL;
            } else {
                if(!zstr(js_curl->credentials)) {
                    copy = strcmp(js_curl->credentials, str);
                }
                if(copy) {
                    js_curl->credentials = switch_core_strdup(js_curl->pool, str);
                }
            }
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_CONTENT_TYPE: {
            str = JS_ToCString(ctx, val);
            if(zstr(str)) {
                js_curl->credentials = NULL;
            } else {
                if(!zstr(js_curl->content_type)) {
                    copy = strcmp(js_curl->content_type, str);
                }
                if(copy) {
                    js_curl->content_type = switch_core_strdup(js_curl->pool, str);
                }
            }
            JS_FreeCString(ctx, str);
            return JS_TRUE;
        }
        case PROP_TIMEOUT: {
            JS_ToUint32(ctx, &js_curl->timeout, val);
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

static JSValue js_curl_do_request(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    struct curl_slist *headers = NULL;
    switch_CURL *curl_handle = NULL;
    const char *req_data = NULL;
    char *durl = NULL, *url_p = NULL;
    char ct[128] = "";
    long http_resp = 0;
    JSValue ret_value = JS_UNDEFINED;

    CURL_SANITY_CHECK();

    if(argc > 0) {
        req_data = JS_ToCString(ctx, argv[0]);
    }

    curl_handle = switch_curl_easy_init();

    if(js_curl->timeout) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, js_curl->timeout);
    }

    if(!strncasecmp(js_curl->url, "https", 5)) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0);
        switch_curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0);
    }

    if(!zstr(js_curl->credentials)) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
        switch_curl_easy_setopt(curl_handle, CURLOPT_USERPWD, js_curl->credentials);
    }

    if(!zstr(js_curl->content_type)) {
        switch_snprintf(ct, sizeof(ct), "Content-Type: %s", js_curl->content_type);
        headers = curl_slist_append(headers, (char *) ct);
        switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    }

    url_p = js_curl->url;

    if(!strcasecmp(js_curl->method, "POST")) {
        switch_curl_easy_setopt(curl_handle, CURLOPT_POST, 1);
        switch_curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, (req_data ? req_data : ""));
    } else if(!zstr(req_data)) {
        durl = switch_mprintf("%s?%s", js_curl->url, req_data);
        url_p = durl;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Request: method: [%s] / url: [%s] / data: [%s] / cred: [%s]\n", js_curl->method, url_p, req_data, switch_str_nil(js_curl->credentials));

    js_curl->fl_ignore_rdata = SWITCH_FALSE;
    js_curl->response_length = 0;

    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url_p);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) js_curl);
    switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.x");

    switch_curl_easy_perform(curl_handle);

    switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_resp);
    switch_curl_easy_cleanup(curl_handle);
    curl_slist_free_all(headers);

    ret_value = JS_NewObject(ctx);
    if(http_resp == 200) {
        JS_SetPropertyStr(ctx, ret_value, "error", JS_NewInt32(ctx, 0));
        if(js_curl->response_length) {
            JS_SetPropertyStr(ctx, ret_value, "content", JS_NewStringLen(ctx, js_curl->response_buffer, js_curl->response_length));
            JS_SetPropertyStr(ctx, ret_value, "contentLength", JS_NewInt32(ctx, js_curl->response_length));
        } else {
            JS_SetPropertyStr(ctx, ret_value, "content", JS_UNDEFINED);
            JS_SetPropertyStr(ctx, ret_value, "contentLength", JS_NewInt32(ctx, 0));
        }
    } else {
        JS_SetPropertyStr(ctx, ret_value, "error", JS_NewInt32(ctx, http_resp));
        JS_SetPropertyStr(ctx, ret_value, "content", JS_UNDEFINED);
        JS_SetPropertyStr(ctx, ret_value, "contentLength", JS_NewInt32(ctx, 0));
    }

    if(js_curl->response_buffer) {
        switch_safe_free(js_curl->response_buffer);
        js_curl->response_buffer = NULL;
    }

    switch_safe_free(durl);
    JS_FreeCString(ctx, req_data);

    return ret_value;
}

static JSValue js_curl_upload_file(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_get_classid(ctx));
    //
    // todo
    //
    return JS_ThrowTypeError(ctx, "Not yet implemented");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static size_t write_callback(void *ptr, size_t size, size_t nmemb, void *udata) {
    js_curl_t *js_curl = (js_curl_t *)udata;
    uint32_t realsize = (size * nmemb);

    if(!js_curl || !realsize || js_curl->fl_ignore_rdata) {
        js_curl->response_length = 0;
        return 0;
    }

    switch_zmalloc(js_curl->response_buffer, realsize + 1);
    memcpy(js_curl->response_buffer, ptr, realsize);
    js_curl->response_length = realsize;

    return realsize;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_curl_class = {
    CLASS_NAME,
    .finalizer = js_curl_finalizer,
};

static const JSCFunctionListEntry js_curl_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("isReady", js_curl_property_get, js_curl_property_set, PROP_IS_READY),
    JS_CGETSET_MAGIC_DEF("url", js_curl_property_get, js_curl_property_set, PROP_URL),
    JS_CGETSET_MAGIC_DEF("method", js_curl_property_get, js_curl_property_set, PROP_METHOD),
    JS_CGETSET_MAGIC_DEF("timeout", js_curl_property_get, js_curl_property_set, PROP_TIMEOUT),
    JS_CGETSET_MAGIC_DEF("credentials", js_curl_property_get, js_curl_property_set, PROP_CREDENTIALS),
    JS_CGETSET_MAGIC_DEF("contentType", js_curl_property_get, js_curl_property_set, PROP_CONTENT_TYPE),
    //
    JS_CFUNC_DEF("doRequest", 1, js_curl_do_request),
    JS_CFUNC_DEF("uploadFile", 1, js_curl_upload_file),
};

static void js_curl_finalizer(JSRuntime *rt, JSValue val) {
    js_curl_t *js_curl = JS_GetOpaque(val, js_lookup_classid(rt, CLASS_NAME));
    switch_memory_pool_t *pool = (js_curl ? js_curl->pool : NULL);

    if(!js_curl) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-curl-finalizer: js_curl=%p\n", js_curl);

    if(js_curl->response_buffer) {
        switch_safe_free(js_curl->response_buffer);
        js_curl->response_buffer = NULL;
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_curl);
}

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

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    url = JS_ToCString(ctx, argv[0]);
    if(zstr(url)) {
        err = JS_ThrowTypeError(ctx, "Invalid argument: url");
        goto fail;
    }

    if(argc > 1) {
        method = JS_ToCString(ctx, argv[1]);
    }

    if(argc > 2) {
        JS_ToUint32(ctx, &timeout, argv[2]);
    }

    if(argc > 3) {
        credentials = JS_ToCString(ctx, argv[3]);
    }

    if(argc > 4) {
        content_type = JS_ToCString(ctx, argv[4]);
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    js_curl = js_mallocz(ctx, sizeof(js_curl_t));
    if(!js_curl) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }

    js_curl->pool = pool;
    js_curl->url = switch_core_strdup(pool, url);
    js_curl->method = (method ? switch_core_strdup(pool, method) : "GET");
    js_curl->credentials = (credentials ? switch_core_strdup(pool, credentials) : NULL);
    js_curl->content_type = (content_type ? switch_core_strdup(pool, content_type) : DEFAULT_CONTENT_TYPE);
    js_curl->timeout = timeout;

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

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-curl-constructor: js_curl=%p\n", js_curl);

    return obj;
fail:
    if(js_curl) {
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
JSClassID js_curl_get_classid(JSContext *ctx) {
    return js_lookup_classid(JS_GetRuntime(ctx), CLASS_NAME);
}

switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj) {
    JSClassID class_id = 0;
    JSValue obj_proto, obj_class;

    class_id = js_curl_get_classid(ctx);
    if(!class_id) {
        JS_NewClassID(&class_id);
        JS_NewClass(JS_GetRuntime(ctx), class_id, &js_curl_class);

        if(js_register_classid(JS_GetRuntime(ctx), CLASS_NAME, class_id) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't register class: %s (%i)\n", CLASS_NAME, (int) class_id);
        }
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_curl_proto_funcs, ARRAY_SIZE(js_curl_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_curl_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

#endif // JS_CURL_ENABLE
