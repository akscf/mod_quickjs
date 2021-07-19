/**
 * FileIO
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME               "FileIO"
#define PROP_PATH                0
#define PROP_OPEN                1

static JSClassID js_fileio_class_id;
static void js_fileio_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_fileio_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_fileio_t *js_fileio = JS_GetOpaque2(ctx, this_val, js_fileio_class_id);

    if(!js_fileio) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_PATH: {
            return JS_NewString(ctx, js_fileio->path);
        }
        case PROP_OPEN: {
            return (js_fileio->fd ? JS_TRUE : JS_FALSE);
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_fileio_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_fileio_t *js_fileio = JS_GetOpaque2(ctx, this_val, js_fileio_class_id);

    return JS_FALSE;
}

static JSValue js_fileio_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_fileio_t *js_fileio = JS_GetOpaque2(ctx, this_val, js_fileio_class_id);
    switch_size_t read = 0;
    int32_t bytes = 0;

    if(!js_fileio || !js_fileio->fd) {
        return JS_FALSE;
    }

    if(!(js_fileio->flags & SWITCH_FOPEN_READ)) {
        return JS_FALSE;
    }

    if(argc > 0) {
        JS_ToUint32(ctx, &bytes, argv[0]);
    }

    if(bytes) {
        if(!js_fileio->buf || js_fileio->buflen < bytes) {
            js_fileio->buf = switch_core_alloc(js_fileio->pool, bytes);
            js_fileio->bufsize = bytes;
        }
        read = bytes;
        switch_file_read(js_fileio->fd, js_fileio->buf, &read);
        js_fileio->buflen = read;

        return (read > 0 ? JS_TRUE : JS_FALSE);
    }

    return JS_FALSE;
}

static JSValue js_fileio_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_fileio_t *js_fileio = JS_GetOpaque2(ctx, this_val, js_fileio_class_id);
    JSValue ret_val = JS_FALSE;
    switch_size_t wrote = 0;
    const char *data = NULL;

    if(!js_fileio || !js_fileio->fd) {
        return JS_FALSE;
    }

    if(!(js_fileio->flags & SWITCH_FOPEN_WRITE)) {
        return JS_FALSE;
    }

    if(argc > 0) {
        data = JS_ToCString(ctx, argv[0]);
    }

    if(!zstr(data)) {
        wrote = strlen(data);
        if(switch_file_write(js_fileio->fd, data, &wrote) == SWITCH_STATUS_SUCCESS) {
            ret_val = JS_TRUE;
        }
        JS_FreeCString(ctx, data);
    }

    return ret_val;
}

static JSValue js_fileio_data(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_fileio_t *js_fileio = JS_GetOpaque2(ctx, this_val, js_fileio_class_id);

    if(!js_fileio || !js_fileio->fd) {
        return JS_UNDEFINED;
    }

    if(js_fileio->buflen) {
        return JS_NewStringLen(ctx, js_fileio->buf, js_fileio->buflen);
    }

    return JS_UNDEFINED;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_fileio_class = {
    CLASS_NAME,
    .finalizer = js_fileio_finalizer,
};

static const JSCFunctionListEntry js_fileio_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("path", js_fileio_property_get, js_fileio_property_set, PROP_PATH),
    JS_CGETSET_MAGIC_DEF("open", js_fileio_property_get, js_fileio_property_set, PROP_OPEN),
    //
    JS_CFUNC_DEF("read", 1, js_fileio_read),
    JS_CFUNC_DEF("write", 1, js_fileio_write),
    JS_CFUNC_DEF("data", 1, js_fileio_data),
};

static void js_fileio_finalizer(JSRuntime *rt, JSValue val) {
    js_fileio_t *js_fileio = JS_GetOpaque(val, js_fileio_class_id);
    switch_memory_pool_t *pool = (js_fileio ? js_fileio->pool : NULL);

    if(!js_fileio) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-fileio-finalizer: js_fileio=%p, fd=%p\n", js_fileio, js_fileio->fd);

    if(js_fileio->fd) {
        switch_file_close(js_fileio->fd);
    }

    if(pool) {
        switch_core_destroy_memory_pool(&pool);
        pool = NULL;
    }

    js_free_rt(rt, js_fileio);
}

static JSValue js_fileio_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_fileio_t *js_fileio = NULL;
    switch_memory_pool_t *pool = NULL;
    switch_file_t *fd = NULL;
    uint32_t flags = 0x0;
    const char *path = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    path = JS_ToCString(ctx, argv[0]);
    if(zstr(path)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: filename");
    }

    if(argc > 1) {
        const char *fstr = JS_ToCString(ctx, argv[1]);

        if(strchr(fstr, 'r')) { flags |= SWITCH_FOPEN_READ; }
        if(strchr(fstr, 'w')) { flags |= SWITCH_FOPEN_WRITE; }
        if(strchr(fstr, 'c')) { flags |= SWITCH_FOPEN_CREATE; }
        if(strchr(fstr, 'a')) { flags |= SWITCH_FOPEN_APPEND; }
        if(strchr(fstr, 't')) { flags |= SWITCH_FOPEN_TRUNCATE; }
        if(strchr(fstr, 'b')) { flags |= SWITCH_FOPEN_BINARY; }

        JS_FreeCString(ctx, fstr);
    } else {
        flags = SWITCH_FOPEN_READ | SWITCH_FOPEN_BINARY;
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    js_fileio = js_mallocz(ctx, sizeof(js_fileio_t));
    if(!js_fileio) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }
    js_fileio->pool = pool;
    js_fileio->path = switch_core_strdup(pool, path);
    js_fileio->flags = flags;

    if(switch_file_open(&js_fileio->fd, path, flags, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, pool) == SWITCH_STATUS_SUCCESS) {
        fd = js_fileio->fd;
        goto fail;
    }

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_fileio_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_fileio);
    JS_FreeCString(ctx, path);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-fileio-constructor: js-fileio=%p, fd=%p\n", js_fileio, js_fileio->fd);

    return obj;
fail:
    JS_FreeCString(ctx, path);
    if(fd) {
        switch_file_close(fd);
    }
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_fileio) {
        js_free(ctx, js_fileio);
    }
    JS_FreeValue(ctx, obj);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_fileio_class_get_id() {
    return js_fileio_class_id;
}

switch_status_t js_fileio_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_fileio_class_id) {
        JS_NewClassID(&js_fileio_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_fileio_class_id, &js_fileio_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_fileio_proto_funcs, ARRAY_SIZE(js_fileio_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_fileio_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_fileio_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

