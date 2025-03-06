/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_FILE_H
#define JS_FILE_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 is_open;
    uint8_t                 tmp_file;
    uint8_t                 type;
    uint32_t                flags;
    switch_size_t           rdbuf_size;
    char                    *path;
    char                    *name;
    char                    *rdbuf;
    switch_file_t           *fd;
    switch_dir_t            *dir;
    switch_memory_pool_t    *pool;
} js_file_t;

JSClassID js_file_get_classid(JSContext *ctx);
JSClassID js_file_get_classid2(JSRuntime *rt);
switch_status_t js_file_class_register(JSContext *ctx, JSValue global_obj, JSClassID class_id);


#endif

