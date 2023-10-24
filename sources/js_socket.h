/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_SOCKET_H
#define JS_SOCKET_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 opened;
    uint8_t                 type;
    uint8_t                 nonblock;
    uint8_t                 mcttl;
    uint32_t                timeout;
    switch_sockaddr_t       *toaddr;
    switch_sockaddr_t       *loaddr;
    switch_size_t           buffer_size;
    switch_memory_pool_t    *pool;
    switch_socket_t         *socket;
    char                    *read_buffer;
} js_socket_t;

JSClassID js_socket_get_classid(JSContext *ctx);
switch_status_t js_socket_class_register(JSContext *ctx, JSValue global_obj);


#endif


