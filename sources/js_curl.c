/**
 * Curl
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/

#include "mod_quickjs.h"
#ifdef JS_CURL_ENABLE

#define CLASS_NAME          "CURL"
#define PROP_READY          0
#define PROP_URL            1
#define PROP_METHOD         2
#define PROP_TIMEOUT        3
#define PROP_CREDENTIALS    4
#define PROP_CONTENT_TYPE   5

#define CURL_SANITY_CHECK() if (!js_curl) { \
           return JS_ThrowTypeError(ctx, "CURL is not initialized"); \
        }

static JSClassID js_curl_class_id;
static void js_curl_finalizer(JSRuntime *rt, JSValue val);
static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_curl_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_class_id);

    if(magic == PROP_READY) {
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
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_class_id);
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
    js_curl_t *js_curl = JS_GetOpaque2(ctx, this_val, js_curl_class_id);
    struct curl_slist *headers = NULL;
    switch_CURL *curl_handle = NULL;
    const char *req_data = NULL;
    char *durl = NULL, *url_p = NULL;
    char ct[128] = "";
    long http_resp = 0;
    JSValue result = JS_UNDEFINED;

    CURL_SANITY_CHECK();

    if(argc > 1) {
        req_data = JS_ToCString(ctx, argv[0]);
    }

    if(!JS_IsUndefined(js_curl->callback)) {
        js_curl->callback = JS_UNDEFINED;
    }
    if(argc > 2 && JS_IsFunction(ctx, argv[2])) {
        js_curl->ctx = ctx;
        js_curl->callback = argv[2];
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
        switch_curl_easy_setopt(curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
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

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Request: method: [%s] url: [%s] data: [%s] cred: [%s]\n", js_curl->method, url_p, req_data, switch_str_nil(js_curl->credentials));

    switch_curl_easy_setopt(curl_handle, CURLOPT_URL, url_p);
    switch_curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, file_callback);
    switch_curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) js_curl);
    switch_curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "freeswitch-curl/1.x");

    switch_curl_easy_perform(curl_handle);

    switch_curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &http_resp);
    switch_curl_easy_cleanup(curl_handle);
    curl_slist_free_all(headers);

out:
    switch_safe_free(durl);
    JS_FreeCString(ctx, req_data);
    return JS_NewInt32(ctx, http_resp);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static size_t file_callback(void *ptr, size_t size, size_t nmemb, void *data) {
    js_curl_t *js_curl = data;
    JSContext *ctx = (js_curl ? js_curl->ctx : NULL);
    uint32_t realsize = (size * nmemb);
    JSValue args[1] = { 0 };
    JSValue ret_val;

    if(!js_curl || !ctx) {
        return 0;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "file_callback, ptr=%p, realsize=%i\n", ptr, realsize);

    if(!JS_IsUndefined(js_curl->callback)) {
        if(ptr) {
            args[0] = JS_NewString(ctx, ptr);
        } else {
            args[0] = JS_NewString(ctx, "");
        }

        ret_val = JS_Call(ctx, js_curl->callback, JS_UNDEFINED, 1, (JSValueConst *) args);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(NULL, NULL, ctx);
            JS_ResetUncatchableError(ctx);
            realsize = 0;
        }

        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, ret_val);
    }

    return realsize;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_curl_class = {
    CLASS_NAME,
    .finalizer = js_curl_finalizer,
};

static const JSCFunctionListEntry js_curl_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("ready", js_curl_property_get, js_curl_property_set, PROP_READY),
    JS_CGETSET_MAGIC_DEF("url", js_curl_property_get, js_curl_property_set, PROP_URL),
    JS_CGETSET_MAGIC_DEF("method", js_curl_property_get, js_curl_property_set, PROP_METHOD),
    JS_CGETSET_MAGIC_DEF("timeout", js_curl_property_get, js_curl_property_set, PROP_TIMEOUT),
    JS_CGETSET_MAGIC_DEF("credentials", js_curl_property_get, js_curl_property_set, PROP_CREDENTIALS),
    JS_CGETSET_MAGIC_DEF("contentType", js_curl_property_get, js_curl_property_set, PROP_CONTENT_TYPE),
    //
    JS_CFUNC_DEF("doRequest", 1, js_curl_do_request),
};

static void js_curl_finalizer(JSRuntime *rt, JSValue val) {
    js_curl_t *js_curl = JS_GetOpaque(val, js_curl_class_id);
    switch_memory_pool_t *pool = (js_curl ? js_curl->pool : NULL);

    if(!js_curl) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-curl-finalizer: js_curl=%p\n", js_curl);

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
    const char *method = NULL;
    const char *url = NULL;

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
        content_type = JS_ToCString(ctx, argv[2]);
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
    js_curl->method = (method ? switch_core_strdup(pool, method) : "POST");
    js_curl->content_type = (content_type ? switch_core_strdup(pool, content_type) : "application/x-www-form-urlencoded");

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_curl_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_curl);

    JS_FreeCString(ctx, url);
    JS_FreeCString(ctx, method);
    JS_FreeCString(ctx, content_type);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-curl-constructor: js_curl=%p\n", js_curl);

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
    JS_FreeCString(ctx, content_type);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_curl_class_get_id() {
    return js_curl_class_id;
}

switch_status_t js_curl_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_curl_class_id) {
        JS_NewClassID(&js_curl_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_curl_class_id, &js_curl_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_curl_proto_funcs, ARRAY_SIZE(js_curl_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_curl_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_curl_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

#endif // JS_CURL_ENABLE
