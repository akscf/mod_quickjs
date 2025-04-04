/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#include "js_socket.h"

#define CLASS_NAME          "Socket"
#define PROP_TYPE           0
#define PROP_TIMEOUT        1
#define PROP_NONBLOCK       2
#define PROP_MCTTL          3

#define S_TYPE_TCP          1
#define S_TYPE_UDP          2
#define S_TYPE_MULTICAST    3

#define SOCKET_SANITY_CHECK() if (!js_socket || !js_socket->socket) { \
           return JS_ThrowTypeError(ctx, "Socket is not initialized"); \
        }

static JSValue js_socket_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);
static void js_socket_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_socket_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));

    if(!js_socket) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_TYPE: {
            switch(js_socket->type) {
                case S_TYPE_UDP:
                    return JS_NewString(ctx, "udp");
                case S_TYPE_TCP:
                    return JS_NewString(ctx, "tcp");
                case S_TYPE_MULTICAST:
                    return JS_NewString(ctx, "multicast");
            }
            return JS_UNDEFINED;
        }
        case PROP_TIMEOUT: {
            return JS_NewInt32(ctx, js_socket->timeout);
        }
        case PROP_NONBLOCK: {
            return (js_socket->nonblock ? JS_TRUE : JS_FALSE);
        }
        case PROP_MCTTL: {
            return JS_NewInt32(ctx, js_socket->mcttl);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_socket_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));

    if(!js_socket || js_socket->socket) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_TIMEOUT: {
            JS_ToUint32(ctx, &js_socket->timeout, val);
            switch_socket_timeout_set(js_socket->socket, js_socket->timeout);
            return JS_TRUE;
        }
        case PROP_NONBLOCK: {
            int bval = JS_ToBool(ctx, val);
            if(switch_socket_opt_set(js_socket->socket, SWITCH_SO_NONBLOCK, bval) == SWITCH_STATUS_SUCCESS) {
                js_socket->nonblock = bval;
            }
            return (js_socket->nonblock ? JS_TRUE : JS_FALSE);
        }
        case PROP_MCTTL: {
            uint32_t ttl = 0;
            JS_ToUint32(ctx, &ttl, val);
            js_socket->mcttl = ttl;
            return JS_TRUE;
        }
    }

    return JS_FALSE;
}

/* connect(hostName, port) */
static JSValue js_socket_connect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));
    switch_status_t status;
    switch_sockaddr_t *addr = NULL;
    const char *host = NULL;
    uint32_t port = 0;
    uint8_t success = 0;

    SOCKET_SANITY_CHECK();

    if(argc < 2) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(js_socket->opened) {
        return JS_TRUE;
    }

    if(QJS_IS_NULL(argv[0])) {
        if(js_socket->type == S_TYPE_MULTICAST) {
            return JS_ThrowTypeError(ctx, "Invalid argument: multicastGroup");
        } else {
            return JS_ThrowTypeError(ctx, "Invalid argument: hostName");
        }
    } else {
        host = JS_ToCString(ctx, argv[0]);
    }

    JS_ToUint32(ctx, &port, argv[1]);
    if(port <= 0 ) {
        return JS_ThrowTypeError(ctx, "Invalid argument: port");
    }

    if(js_socket->type == S_TYPE_MULTICAST) {
        if((status = switch_sockaddr_info_get(&js_socket->toaddr, host, SWITCH_UNSPEC, port, 0, js_socket->pool)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_sockaddr_info_get failed (err: %d)\n", status);
            goto out;
        }
        if((status = switch_mcast_interface(js_socket->socket, js_socket->loaddr)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_mcast_interface failed (err: %d)\n", status);
            goto out;
        }
        if((status = switch_mcast_join(js_socket->socket, js_socket->toaddr, NULL, NULL)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_mcast_join failed (err: %d)\n", status);
            goto out;
        }
        if((status = switch_mcast_hops(js_socket->socket, js_socket->mcttl)) != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_mcast_hops failed (err: %d)\n", status);
            goto out;
        }

        success = 1;
        js_socket->opened = SWITCH_TRUE;
        goto out;
    }

    if(js_socket->type == S_TYPE_UDP) {
        status = switch_sockaddr_info_get(&js_socket->toaddr, host, SWITCH_UNSPEC, port, 0, js_socket->pool);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_sockaddr_info_get failed (err: %d)\n", status);
            goto out;
        }

        success = 1;
        js_socket->opened = SWITCH_TRUE;
        goto out;
    }

    if(js_socket->type == S_TYPE_TCP) {
        status = switch_sockaddr_info_get(&addr, host, AF_INET, (switch_port_t) port, 0, js_socket->pool);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_sockaddr_info_get failed (err: %d)\n", status);
            goto out;
        }

        switch_socket_opt_set(js_socket->socket, SWITCH_SO_KEEPALIVE, 1);
        switch_socket_opt_set(js_socket->socket, SWITCH_SO_TCP_NODELAY, 1);

        status = switch_socket_connect(js_socket->socket, addr);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_socket_connect failed (err: %d)\n", status);
            goto out;
        }

        success = 1;
        js_socket->opened = SWITCH_TRUE;
        goto out;
    }

out:
    if(success && js_socket->nonblock) {
        switch_socket_opt_set(js_socket->socket, SWITCH_SO_NONBLOCK, 1);
    }

    JS_FreeCString(ctx, host);
    return (success ? JS_TRUE : JS_FALSE);
}

static JSValue js_socket_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));

    if(!js_socket) {
        return JS_ThrowTypeError(ctx, "Socket not initialized");
    }
    if(!js_socket->opened) {
        return JS_TRUE;
    }

    if(js_socket->socket) {
        switch_socket_shutdown(js_socket->socket, SWITCH_SHUTDOWN_READWRITE);
        switch_socket_close(js_socket->socket);
        js_socket->opened = SWITCH_FALSE;
        js_socket->socket = NULL;

        return JS_TRUE;
    }

    return JS_FALSE;
}

/* write(buffer, len) */
static JSValue js_socket_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &buf_size, argv[0]);
    if(!buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: buffer");
    }

    JS_ToInt64(ctx, &len, argv[1]);
    if(len <= 0) {
        return JS_NewInt64(ctx, 0);
    }
    if(len > buf_size) {
        return JS_ThrowRangeError(ctx, "len > buffer.size");
    }

    if(js_socket->type == S_TYPE_UDP) {
        if(switch_socket_sendto(js_socket->socket, js_socket->toaddr, 0, buf, &len) != SWITCH_STATUS_SUCCESS) {
            if(!len) { return JS_NewInt64(ctx, 0); }
            return JS_EXCEPTION;
        }
        return JS_NewInt64(ctx, len);
    }

    if(js_socket->type == S_TYPE_TCP) {
        if(switch_socket_send(js_socket->socket, buf, &len) != SWITCH_STATUS_SUCCESS) {
            if(!len) { return JS_NewInt64(ctx, 0); }
            return JS_EXCEPTION;
        }
        return JS_NewInt64(ctx, len);
    }

    return JS_NewInt64(ctx, 0);
}

/* read(buf, len) */
static JSValue js_socket_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));
    switch_size_t buf_size = 0;
    switch_size_t len = 0;
    uint8_t *buf = NULL;

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &buf_size, argv[0]);
    if(!buf) {
        return JS_ThrowTypeError(ctx, "Invalid argument: buffer");
    }

    JS_ToInt64(ctx, &len, argv[1]);
    if(len <= 0) {
        return JS_NewInt64(ctx, 0);
    }
    if(len > buf_size) {
        return JS_ThrowRangeError(ctx, "len > buffer.size");
    }

    if(switch_socket_recv(js_socket->socket, buf, &len) != SWITCH_STATUS_SUCCESS) {
        if(!len) { return JS_NewInt64(ctx, 0); }
        return JS_EXCEPTION;
    }

    return JS_NewInt64(ctx, len);
}

/* writeString(str) */
static JSValue js_socket_write_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));
    uint32_t success = 0;
    switch_size_t len = 0;
    const char *data = NULL;

    SOCKET_SANITY_CHECK();

    if(!js_socket->opened) {
        return JS_ThrowTypeError(ctx, "Socket is closed");
    }
    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Not enough arguments");
    }

    if(QJS_IS_NULL(argv[0])) {
        success = 0; goto out;
    }

    data = JS_ToCString(ctx, argv[0]);
    len = strlen(data);

    if(js_socket->type == S_TYPE_UDP) {
        if(switch_socket_sendto(js_socket->socket, js_socket->toaddr, 0, data, &len) == SWITCH_STATUS_SUCCESS) {
            success = 1;
        }
    }

    if(js_socket->type == S_TYPE_TCP) {
        if(switch_socket_send(js_socket->socket, data, &len) == SWITCH_STATUS_SUCCESS) {
            success = 1;
        }
    }
out:
    JS_FreeCString(ctx, data);
    return (success ? JS_TRUE : JS_FALSE);
}

/* readString() */
static JSValue js_socket_read_string(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_socket_t *js_socket = JS_GetOpaque2(ctx, this_val, js_socket_get_classid(ctx));
    const char *usr_delimiter = NULL;
    const char *def_delimiter = "\n";
    char tmp[1024] = "";
    uint32_t ofs = 0, i = 0, found = 0;
    switch_size_t len = 0;
    switch_size_t total_length = 0;
    switch_status_t status = SWITCH_STATUS_FALSE;

    SOCKET_SANITY_CHECK();

    if(!js_socket->opened) {
        return JS_ThrowTypeError(ctx, "Socket is closed");
    }
    if(argc > 0) {
        usr_delimiter = JS_ToCString(ctx, argv[0]);
    }

    if(!js_socket->read_buffer) {
        js_socket->buffer_size = 4096;
        js_socket->read_buffer = switch_core_alloc(js_socket->pool, js_socket->buffer_size);
    }

    while(1) {
        len = sizeof(tmp);
        status = switch_socket_recv(js_socket->socket, tmp, &len);
        if(status != SWITCH_STATUS_SUCCESS || !len) {
            break;
        }

        for(i = 0; i < len; i++) {
            const char *delim = (usr_delimiter ? usr_delimiter : def_delimiter);
            if(tmp[i] == delim[0]) {
                found = 1; len = i; break;
            }
        }

        ofs = total_length;
        total_length += len;
        if(total_length > js_socket->buffer_size) {
            char *new_buf = switch_core_alloc(js_socket->pool, total_length);
            memcpy(new_buf, js_socket->read_buffer, js_socket->buffer_size);
            memcpy(new_buf + js_socket->buffer_size, tmp, len);
            js_socket->buffer_size = total_length;
            js_socket->read_buffer = new_buf;
        } else {
            memcpy(js_socket->read_buffer + ofs , tmp, len);

        }
        if(found) {
            break;
        }
    }

    JS_FreeCString(ctx, usr_delimiter);

    if(status == SWITCH_STATUS_SUCCESS && total_length > 0) {
        return JS_NewStringLen(ctx, js_socket->read_buffer, total_length);
    }

    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_socket_class = {
    CLASS_NAME,
    .finalizer = js_socket_finalizer,
};

static const JSCFunctionListEntry js_socket_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("type", js_socket_property_get, js_socket_property_set, PROP_TYPE),
    JS_CGETSET_MAGIC_DEF("nonblock", js_socket_property_get, js_socket_property_set, PROP_NONBLOCK),
    JS_CGETSET_MAGIC_DEF("timeout", js_socket_property_get, js_socket_property_set, PROP_TIMEOUT),
    JS_CGETSET_MAGIC_DEF("ttl", js_socket_property_get, js_socket_property_set, PROP_MCTTL),
    //
    JS_CFUNC_DEF("connect", 1, js_socket_connect),
    JS_CFUNC_DEF("close", 1, js_socket_close),
    JS_CFUNC_DEF("write", 2, js_socket_write),
    JS_CFUNC_DEF("read", 2, js_socket_read),
    JS_CFUNC_DEF("writeString", 1, js_socket_write_string),
    JS_CFUNC_DEF("readString", 1, js_socket_read_string),
};

static void js_socket_finalizer(JSRuntime *rt, JSValue val) {
    js_socket_t *js_socket = JS_GetOpaque(val, js_socket_get_classid2(rt));

    if(!js_socket) {
        return;
    }

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-socket-finalizer: js_socket=%p, socket=%p\n", js_socket, js_socket->socket);
#endif

    if(js_socket->socket) {
        switch_socket_shutdown(js_socket->socket, SWITCH_SHUTDOWN_READWRITE);
        switch_socket_close(js_socket->socket);
    }

    if(js_socket->pool) {
        switch_core_destroy_memory_pool(&js_socket->pool);
    }

    js_free_rt(rt, js_socket);
}

/* new() */
static JSValue js_socket_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    switch_status_t status;
    js_socket_t *js_socket = NULL;
    switch_socket_t *socket = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_sockaddr_t *loaddr = NULL;
    const char *lo_addr_str = NULL;
    const char *mc_addr_str = NULL;
    uint32_t timeout = 0;
    int type = S_TYPE_TCP;
    int nonblock = 0;

    if(argc > 0) {
        const char *type_str = JS_ToCString(ctx, argv[0]);
        if(!zstr(type_str)) {
            if(strcasecmp(type_str, "udp") == 0) {
                type = S_TYPE_UDP;
            } else if(strcasecmp(type_str, "tcp") == 0) {
                type = S_TYPE_TCP;
            }
        }
        JS_FreeCString(ctx, type_str);
    }

    if(argc > 2) {
        nonblock = JS_ToBool(ctx, argv[2]);
    }

    js_socket = js_mallocz(ctx, sizeof(js_socket_t));
    if(!js_socket) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "js_mallocz()\n");
        return JS_EXCEPTION;
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "switch_core_new_memory_pool()\n");
        goto fail;
    }

    if(type == S_TYPE_UDP || type == S_TYPE_MULTICAST) {
        if(argc > 1 ){
            lo_addr_str = JS_ToCString(ctx, argv[1]);
            if(!zstr(lo_addr_str)) {
                status = switch_sockaddr_info_get(&loaddr, lo_addr_str, SWITCH_UNSPEC, 0, 0, pool);
                if(status != SWITCH_STATUS_SUCCESS) {
                    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_sockaddr_info_get failed (err: %d)\n", status);
                    err = JS_ThrowTypeError(ctx, "Couldn't create socket");
                    goto fail;
                }
            }
        }
        if(!loaddr) {
            err = JS_ThrowTypeError(ctx, "Invalid argument: localAddress");
            goto fail;
        }

        status = switch_socket_create(&socket, switch_sockaddr_get_family(loaddr), SOCK_DGRAM, 0, pool);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_socket_create failed (err: %d)\n", status);
            err = JS_ThrowTypeError(ctx, "Couldn't create socket");
            goto fail;
        }

        status = switch_socket_bind(socket, loaddr);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_socket_bind failed (err: %d)\n", status);
            err = JS_ThrowTypeError(ctx, "Couldn't create socket");
            goto fail;
        }
    }

    if(type == S_TYPE_TCP) {
        if(argc > 1) {
            if(JS_IsNumber(argv[1])) {
                JS_ToUint32(ctx, &timeout, argv[1]);
            }
        }

        status = switch_socket_create(&socket, AF_INET, SOCK_STREAM, SWITCH_PROTO_TCP, pool);
        if(status != SWITCH_STATUS_SUCCESS) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "switch_socket_create failed (err: %d)\n", status);
            err = JS_ThrowTypeError(ctx, "Couldn't create socket");
            goto fail;
        }
    }

    if(socket) {
        if(timeout > 0) {
            switch_socket_timeout_set(socket, timeout);
        }
    }

    js_socket->mcttl = 1;
    js_socket->pool = pool;
    js_socket->type = type;
    js_socket->loaddr = loaddr;
    js_socket->socket = socket;
    js_socket->nonblock = nonblock;
    js_socket->timeout = timeout;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_socket_get_classid(ctx));
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_socket);
    JS_FreeCString(ctx, lo_addr_str);
    JS_FreeCString(ctx, mc_addr_str);

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "js-socket-constructor: js_socket=%p, socket=%p\n", js_socket, js_socket->socket);
#endif

    return obj;

fail:
    if(js_socket) {
        if(js_socket->socket) {
            switch_socket_shutdown(js_socket->socket, SWITCH_SHUTDOWN_READWRITE);
            switch_socket_close(js_socket->socket);
        }
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_socket) {
        js_free(ctx, js_socket);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, mc_addr_str);
    JS_FreeCString(ctx, lo_addr_str);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_socket_get_classid2(JSRuntime *rt) {
    script_t *script = JS_GetRuntimeOpaque(rt);
    switch_assert(script);
    return script->class_id_socket;
}
JSClassID js_socket_get_classid(JSContext *ctx) {
    return  js_socket_get_classid2(JS_GetRuntime(ctx));
}

switch_status_t js_socket_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id) {
    JSValue obj_proto, obj_class;
    script_t *script = JS_GetRuntimeOpaque(JS_GetRuntime(ctx));

    switch_assert(script);

    if(JS_IsRegisteredClass(JS_GetRuntime(ctx), class_id)) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Class with id (%d) already registered!\n", class_id);
        return SWITCH_STATUS_FALSE;
    }

    JS_NewClassID(&class_id);
    JS_NewClass(JS_GetRuntime(ctx), class_id, &js_socket_class);
    script->class_id_socket = class_id;

#ifdef MOD_QUICKJS_DEBUG
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Class registered [%s / %d]\n", CLASS_NAME, class_id);
#endif

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_socket_proto_funcs, ARRAY_SIZE(js_socket_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_socket_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}
