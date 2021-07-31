// FileIO
typedef struct {
    uint32_t                flags;
    int32_t                 bufsize;
    switch_size_t           buflen;
    char                    *path;
    char                    *buf;
    switch_file_t           *fd;
    switch_memory_pool_t    *pool;
} js_fileio_t;

JSClassID js_fileio_class_get_id();
switch_status_t js_fileio_class_register(JSContext *ctx, JSValue global_obj);
