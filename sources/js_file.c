/**
 * File
 *
 * Copyright (C) AlexandrinKS
 * https://akscf.me/
 **/
#include "mod_quickjs.h"

#define CLASS_NAME               "File"
#define PROP_PATH                0
#define PROP_NAME                1
#define PROP_SIZE                2
#define PROP_POSITION            3
#define PROP_CAN_READ            4
#define PROP_CAN_WRITE           5
#define PROP_IS_OPEN             6
#define PROP_IS_FILE             7
#define PROP_IS_DIRECTORY        8
#define PROP_CREATEION_TIME      9
#define PROP_LAST_MODIFIED       10

#define TYPE_FILE   1
#define TYPE_DIR    2

#define FILE_SANITY_CHECK() if (!js_file) { \
           return JS_ThrowTypeError(ctx, "File is not initialized"); \
        }

#define FILE_SANITY_CHECK_OPEN() if (!js_file || !js_file->fd) { \
           return JS_ThrowTypeError(ctx, "File is not opened"); \
        }

#define DIR_SANITY_CHECK_OPEN() if (!js_file || !js_file->dir) { \
           return JS_ThrowTypeError(ctx, "Direactory is not opened"); \
        }

static JSClassID js_file_class_id;
static void js_file_finalizer(JSRuntime *rt, JSValue val);

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static switch_size_t xx_try_get_size(js_file_t *js_file) {
    switch_file_t *fd = NULL;
    switch_dir_t  *dir = NULL;
    switch_size_t sz = 0;

    if(js_file->type == TYPE_DIR || switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        if(switch_dir_open(&dir, js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
            sz = switch_dir_count(dir);
            switch_dir_close(dir);
        }
    }

    if(switch_file_open(&fd, js_file->path, SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        sz = switch_file_get_size(fd);
        switch_file_close(fd);
        return sz;
    }

    return 0;
}

static uint8_t xx_try_access(js_file_t *js_file, int flags) {
    switch_file_t *fd = NULL;

    if(js_file->type == TYPE_DIR) {
        if(switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
            return SWITCH_TRUE;
        }
        return SWITCH_FALSE;
    }

    if(js_file->type == TYPE_FILE || switch_file_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        if(switch_file_open(&fd, js_file->path, flags, SWITCH_FPROT_UREAD, js_file->pool) == SWITCH_STATUS_SUCCESS) {
            switch_file_close(fd);
            return SWITCH_TRUE;
        }
        return SWITCH_FALSE;
    }

    return SWITCH_FALSE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_file_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);

    if(!js_file) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_PATH: {
            return JS_NewString(ctx, js_file->path);
        }
        case PROP_NAME: {
            return JS_NewString(ctx, js_file->name);
        }
        case PROP_SIZE: {
            switch_size_t sz = 0;
            if(js_file->is_open) {
                if(js_file->fd) {
                    sz = switch_file_get_size(js_file->fd);
                } else if(js_file->dir) {
                    sz = switch_dir_count(js_file->dir);
                }
            } else {
                sz = xx_try_get_size(js_file);
            }
            return JS_NewInt64(ctx, sz);
        }
        case PROP_POSITION: {
            uint64_t pos = 0;
            if(js_file->is_open && js_file->fd) {
                switch_file_seek(js_file->fd, SEEK_CUR, &pos);
            }
            return JS_NewInt64(ctx, pos);
        }
        case PROP_CAN_READ: {
            if(js_file->is_open && js_file->fd) {
                return (js_file->flags & SWITCH_FOPEN_READ) ? JS_TRUE : JS_FALSE;
            }
            return xx_try_access(js_file, SWITCH_FOPEN_READ) ? JS_TRUE : JS_FALSE;
        }
        case PROP_CAN_WRITE: {
            if(js_file->is_open && js_file->fd) {
                return (js_file->flags & SWITCH_FOPEN_READ) ? JS_TRUE : JS_FALSE;
            }
            return xx_try_access(js_file, SWITCH_FOPEN_WRITE) ? JS_TRUE : JS_FALSE;
        }
        case PROP_IS_OPEN: {
            return (js_file->is_open ? JS_TRUE : JS_FALSE);
        }
        case PROP_IS_FILE: {
            if(js_file->type && js_file->type == TYPE_FILE) {
                return JS_TRUE;
            }
            if(js_file->is_open) {
                return (js_file->fd ? JS_TRUE : JS_FALSE);
            }
            if(switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
                return JS_FALSE;
            }
            if(switch_file_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
                js_file->type = TYPE_FILE;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_IS_DIRECTORY: {
            if(js_file->type && js_file->type == TYPE_DIR) {
                return JS_TRUE;
            }
            if(js_file->is_open) {
                return (js_file->dir ? JS_TRUE : JS_FALSE);
            }
            if(switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
                js_file->type = TYPE_DIR;
                return JS_TRUE;
            }
            return JS_FALSE;
        }
        case PROP_CREATEION_TIME: {
            return JS_UNDEFINED;
        }
        case PROP_LAST_MODIFIED: {
            return JS_UNDEFINED;
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_file_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);

    return JS_FALSE;
}


static JSValue js_file_exists(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);

    FILE_SANITY_CHECK();

    if(js_file->is_open) {
        return JS_TRUE;
    }

    if(switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        js_file->type = TYPE_DIR;
        return JS_TRUE;
    }

    if(switch_file_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        js_file->type = TYPE_FILE;
        return JS_TRUE;
    }

    return JS_FALSE;
}

static JSValue js_file_mktemp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    JSValue ret_val = JS_FALSE;

    FILE_SANITY_CHECK();

    if(js_file->is_open) {
        return JS_FALSE;
    }

    js_file->flags = SWITCH_FOPEN_CREATE | SWITCH_FOPEN_WRITE | SWITCH_FOPEN_TRUNCATE | SWITCH_FOPEN_BINARY;
    if(switch_file_mktemp(&js_file->fd, js_file->path, js_file->flags, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        js_file->is_open = SWITCH_TRUE;
        js_file->tmp_file = SWITCH_TRUE;
        ret_val = JS_TRUE;
    }

    return ret_val;
}

static JSValue js_file_open(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    uint32_t flags = 0;

    FILE_SANITY_CHECK();

    if(js_file->is_open) {
        return JS_TRUE;
    }

    if(js_file->type == TYPE_DIR || switch_directory_exists(js_file->path, NULL) == SWITCH_STATUS_SUCCESS) {
        if(switch_dir_open(&js_file->dir, js_file->path, js_file->pool) != SWITCH_STATUS_SUCCESS) {
            return JS_FALSE;
        }
        js_file->is_open = SWITCH_TRUE;
        return JS_TRUE;
    }

    if(argc > 0) {
        const char *fstr = JS_ToCString(ctx, argv[0]);

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

    if(switch_file_open(&js_file->fd, js_file->path, flags, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        js_file->is_open = SWITCH_TRUE;
        return JS_TRUE;
    }
    return JS_FALSE;
}

static JSValue js_file_close(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);

    FILE_SANITY_CHECK();

    if(!js_file->is_open) {
        return JS_TRUE;
    }

    if(js_file->fd) {
        switch_file_close(js_file->fd);
    }

    if(js_file->dir) {
        switch_dir_close(js_file->dir);
    }

    js_file->is_open = SWITCH_FALSE;

    return JS_TRUE;
}

static JSValue js_file_read(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    switch_size_t size = 0, len = 0;
    uint8_t *buf = NULL;

    FILE_SANITY_CHECK_OPEN();

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
    if(!buf) {
        return JS_EXCEPTION;
    }

    JS_ToInt64(ctx, &len, argv[1]);
    if(len <= 0) {
        return JS_NewInt64(ctx, 0);
    }
    if(len > size) {
        return JS_ThrowRangeError(ctx, "Array buffer overflow (len > array size)");
    }

    if(switch_file_read(js_file->fd, buf, &len) != SWITCH_STATUS_SUCCESS) {
        if(len == 0) {
            return JS_NewInt64(ctx, 0);
        }
        return JS_EXCEPTION;
    }

    return JS_NewInt64(ctx, len);
}

static JSValue js_file_write(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    switch_size_t size = 0, len = 0;
    uint8_t *buf = NULL;

    FILE_SANITY_CHECK_OPEN();

    if(argc < 2)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
    if(!buf) {
        return JS_EXCEPTION;
    }

    JS_ToInt64(ctx, &len, argv[1]);
    if(len <= 0) {
        return JS_NewInt64(ctx, 0);
    }

    if(len > size) {
        return JS_ThrowRangeError(ctx, "Array buffer overflow (len > array size)");
    }

    if(switch_file_write(js_file->fd, buf, &len) != SWITCH_STATUS_SUCCESS) {
        return JS_EXCEPTION;
    }

    return JS_NewInt64(ctx, len);
}

static JSValue js_file_write_str(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    switch_status_t status;
    switch_size_t len = 0;
    const char *str = NULL;

    FILE_SANITY_CHECK_OPEN();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    str = JS_ToCString(ctx, argv[0]);
    if(!zstr(str)) {
        return JS_NewInt64(ctx, 0);
    }

    len = strlen(str);
    status = switch_file_write(js_file->fd, str, &len);
    JS_FreeCString(ctx, str);

    return (status == SWITCH_STATUS_SUCCESS ? JS_NewInt64(ctx, len) : JS_EXCEPTION);
}

static JSValue js_file_read_str(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    switch_size_t len = 0;

    FILE_SANITY_CHECK_OPEN();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    JS_ToInt64(ctx, &len, argv[0]);
    if(len <= 0) {
        return JS_UNDEFINED;
    }

    if(!js_file->rdbuf || len > js_file->rdbuf_size) {
        js_file->rdbuf_size = len;
        js_file->rdbuf = switch_core_alloc(js_file->pool, js_file->rdbuf_size);
    }

    if(switch_file_read(js_file->fd, js_file->rdbuf, &len) != SWITCH_STATUS_SUCCESS) {
        if(len == 0) {
            return JS_UNDEFINED;
        }
        return JS_EXCEPTION;
    }

    return JS_NewStringLen(ctx, js_file->rdbuf, len);
}

static JSValue js_file_seek(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    int64_t ofs = 0;


    FILE_SANITY_CHECK_OPEN();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    JS_ToInt64(ctx, &ofs, argv[0]);

    if(switch_file_seek(js_file->fd, SEEK_SET, &ofs) == SWITCH_STATUS_SUCCESS) {
        return JS_TRUE;
    }

    return JS_FALSE;
}

static JSValue js_file_remove(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    char *cmd = NULL;

    FILE_SANITY_CHECK();

    if(js_file->is_open) {
        if(js_file->fd) {
            switch_file_close(js_file->fd);
        }
        if(js_file->dir) {
            switch_dir_close(js_file->dir);
        }
        js_file->is_open = SWITCH_FALSE;
    }

    if(strlen(js_file->path) == 1 && strncmp("/", js_file->path, 1) == 0)  {
        return JS_ThrowTypeError(ctx, "Root dir can't be deleted");
    }

    cmd = switch_mprintf("rm -rf %s", js_file->path);
    system(cmd);

    js_file->type = 0;
    switch_safe_free(cmd);

    return JS_TRUE;
}

static JSValue js_file_rename(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    JSValue ret_val = JS_FALSE;
    const char *to_path = NULL;

    FILE_SANITY_CHECK();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    to_path = JS_ToCString(ctx, argv[0]);
    if(zstr(to_path)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: toPath");
    }

    if(switch_file_rename(js_file->path, to_path, NULL) == SWITCH_STATUS_SUCCESS) {
        js_file->path = switch_core_strdup(js_file->pool, to_path);
        js_file->name = basename(js_file->path);
        ret_val = JS_TRUE;
    }
    JS_FreeCString(ctx, to_path);

    return ret_val;
}

static JSValue js_file_copy(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    JSValue ret_val;
    const char *to_path = NULL;

    FILE_SANITY_CHECK();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    to_path = JS_ToCString(ctx, argv[0]);
    if(zstr(to_path)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: toPath");
    }

    ret_val = ((switch_file_copy(js_file->path, to_path, SWITCH_FPROT_FILE_SOURCE_PERMS, NULL) == SWITCH_STATUS_SUCCESS) ? JS_TRUE : JS_FALSE);
    JS_FreeCString(ctx, to_path);

    return ret_val;
}

static JSValue js_file_mkdir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    JSValue ret_val;

    FILE_SANITY_CHECK();

   if(switch_directory_exists(js_file->path, js_file->pool) == SWITCH_STATUS_SUCCESS) {
        return JS_TRUE;
    }

    ret_val = (switch_dir_make_recursive(js_file->path, SWITCH_DEFAULT_DIR_PERMS, NULL) == SWITCH_STATUS_SUCCESS) ? JS_TRUE : JS_FALSE;

    return ret_val;
}

static JSValue js_file_dir_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    js_file_t *js_file = JS_GetOpaque2(ctx, this_val, js_file_class_id);
    char file_buf[128] = "";
    char path_buf[1024] = "";
    JSValue js_cb;
    JSValue ret_val;
    JSValue args[2] = { 0 };

    DIR_SANITY_CHECK_OPEN();

    if(argc < 1)  {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    if(!JS_IsFunction(ctx, argv[0])) {
        return JS_ThrowTypeError(ctx, "Invalid argument: callback");
    }
    js_cb = argv[0];

    while(1) {
        const char *fname = NULL;

        if(!(fname = switch_dir_next_file(js_file->dir, file_buf, sizeof(file_buf)))) {
            break;
        }
        switch_snprintf(path_buf, sizeof(path_buf), "%s%s%s", js_file->path, SWITCH_PATH_SEPARATOR, fname);

        args[0] = JS_NewString(ctx, path_buf);
        args[1] = JS_NewString(ctx, fname);

        ret_val = JS_Call(ctx, js_cb, this_val, 2, (JSValueConst *) args);
        if(JS_IsException(ret_val)) {
            ctx_dump_error(NULL, NULL, ctx);
            JS_ResetUncatchableError(ctx);
        }

        JS_FreeValue(ctx, args[0]);
        JS_FreeValue(ctx, args[1]);
        JS_FreeValue(ctx, ret_val);
    }

    return JS_TRUE;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_file_class = {
    CLASS_NAME,
    .finalizer = js_file_finalizer,
};

static const JSCFunctionListEntry js_file_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("path", js_file_property_get, js_file_property_set, PROP_PATH),
    JS_CGETSET_MAGIC_DEF("name", js_file_property_get, js_file_property_set, PROP_NAME),
    JS_CGETSET_MAGIC_DEF("size", js_file_property_get, js_file_property_set, PROP_SIZE),
    JS_CGETSET_MAGIC_DEF("position", js_file_property_get, js_file_property_set, PROP_POSITION),
    JS_CGETSET_MAGIC_DEF("canRead", js_file_property_get, js_file_property_set, PROP_CAN_READ),
    JS_CGETSET_MAGIC_DEF("canWrite", js_file_property_get, js_file_property_set, PROP_CAN_WRITE),
    JS_CGETSET_MAGIC_DEF("isOpen", js_file_property_get, js_file_property_set, PROP_IS_OPEN),
    JS_CGETSET_MAGIC_DEF("isFile", js_file_property_get, js_file_property_set, PROP_IS_FILE),
    JS_CGETSET_MAGIC_DEF("isDirectory", js_file_property_get, js_file_property_set, PROP_IS_DIRECTORY),
    JS_CGETSET_MAGIC_DEF("creationTime", js_file_property_get, js_file_property_set, PROP_CREATEION_TIME),
    JS_CGETSET_MAGIC_DEF("lastModified", js_file_property_get, js_file_property_set, PROP_LAST_MODIFIED),
    //
    JS_CFUNC_DEF("exists", 0, js_file_exists),
    JS_CFUNC_DEF("mktemp", 0, js_file_mktemp),
    JS_CFUNC_DEF("open", 1, js_file_open),
    JS_CFUNC_DEF("close", 0, js_file_close),
    JS_CFUNC_DEF("read", 2, js_file_read),
    JS_CFUNC_DEF("write", 2, js_file_write),
    JS_CFUNC_DEF("writeString", 2, js_file_write_str),
    JS_CFUNC_DEF("readString", 1, js_file_read_str),
    JS_CFUNC_DEF("seek", 1, js_file_seek),
    JS_CFUNC_DEF("remove", 0, js_file_remove),
    JS_CFUNC_DEF("rename", 1, js_file_rename),
    JS_CFUNC_DEF("copy", 1, js_file_copy),
    JS_CFUNC_DEF("mkdir", 0, js_file_mkdir),
    JS_CFUNC_DEF("list", 1, js_file_dir_list),
};

static void js_file_finalizer(JSRuntime *rt, JSValue val) {
    js_file_t *js_file = JS_GetOpaque(val, js_file_class_id);

    if(!js_file) {
        return;
    }

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-file-finalizer: js_file=%p, fd=%p, dir=%p\n", js_file, js_file->fd, js_file->dir);

    if(js_file->is_open) {
        if(js_file->fd) {
            switch_file_close(js_file->fd);
        }
        if(js_file->dir) {
            switch_dir_close(js_file->dir);
        }
        js_file->is_open = SWITCH_FALSE;
    }

    if(js_file->tmp_file) {
        switch_file_remove(js_file->path, js_file->pool);
    }

    if(js_file->pool) {
        switch_core_destroy_memory_pool(&js_file->pool);
        js_file->pool = NULL;
    }

    js_free_rt(rt, js_file);
}

static JSValue js_file_contructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSValue obj = JS_UNDEFINED;
    JSValue err = JS_UNDEFINED;
    JSValue proto;
    js_file_t *js_file = NULL;
    switch_memory_pool_t *pool = NULL;
    const char *path = NULL;

    if(argc < 1) {
        return JS_ThrowTypeError(ctx, "Invalid arguments");
    }

    path = JS_ToCString(ctx, argv[0]);
    if(zstr(path)) {
        return JS_ThrowTypeError(ctx, "Invalid argument: filename");
    }

    if(switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool failure\n");
        goto fail;
    }

    js_file = js_mallocz(ctx, sizeof(js_file_t));
    if(!js_file) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "mem fail\n");
        goto fail;
    }
    js_file->pool = pool;
    js_file->path = switch_core_strdup(pool, path);
    js_file->name = basename(js_file->path);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_file_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_file);
    JS_FreeCString(ctx, path);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "js-file-constructor: js-file=%p\n", js_file);

    return obj;
fail:
    if(pool) {
        switch_core_destroy_memory_pool(&pool);
    }
    if(js_file) {
        js_free(ctx, js_file);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, path);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
// Public api
// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
JSClassID js_file_class_get_id() {
    return js_file_class_id;
}

switch_status_t js_file_class_register(JSContext *ctx, JSValue global_obj) {
    JSValue obj_proto;
    JSValue obj_class;

    if(!js_file_class_id) {
        JS_NewClassID(&js_file_class_id);
        JS_NewClass(JS_GetRuntime(ctx), js_file_class_id, &js_file_class);
    }

    obj_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, obj_proto, js_file_proto_funcs, ARRAY_SIZE(js_file_proto_funcs));

    obj_class = JS_NewCFunction2(ctx, js_file_contructor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, obj_class, obj_proto);
    JS_SetClassProto(ctx, js_file_class_id, obj_proto);

    JS_SetPropertyStr(ctx, global_obj, CLASS_NAME, obj_class);

    return SWITCH_STATUS_SUCCESS;
}

