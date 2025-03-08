/**
 **
 ** (C)2025 aks
 **/
#ifndef QJS_PGSQL_H
#define QJS_PGSQL__H

#include <quickjs.h>
#include <libpq-fe.h>

#define QJS_PGSQL_DEBUG

#define QJS_IS_NULL(jsV)    (JS_IsNull(jsV) || JS_IsUndefined(jsV) || JS_IsUninitialized(jsV))
#define ARRAY_SIZE(a)       (sizeof(a) / sizeof((a)[0]))


#ifdef QJS_FREESWITCH
#include <switch.h>
#define log_debug(fmt, ...) do{switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, fmt "\n", ##__VA_ARGS__);} while (0)
#define log_error(fmt, ...) do{switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, fmt "\n", ##__VA_ARGS__);} while (0)

#else

#include <stdio.h>
#define log_debug(fmt, ...) do{printf("DEBUG [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
#define log_error(fmt, ...) do{printf("ERROR [%s:%d]: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);} while (0)
static inline int _zstr(const char *s) { if(!s || *s == '\0') { return 1; } else { return 0; } }
#define zstr(x) _zstr(x)
#define switch_str_nil(s) (s ? s : "")

#endif // QJS_FREESWITCH


typedef struct {
    PGconn      *conn;
    char        *dsn;
    uint8_t     fl_conntected;
} qjs_pgsql_t;





#endif
