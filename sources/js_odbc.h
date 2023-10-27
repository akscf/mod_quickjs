/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_ODBC_H
#define JS_ODBC_H
#include "mod_quickjs.h"

#define JS_ODBC_ENABLE

#ifdef JS_ODBC_ENABLE

#include <sql.h>
#include <sqltypes.h>
#ifdef _MSC_VER
 #pragma warning(push)
 #pragma warning(disable:4201)
 #include <sqlext.h>
 #pragma warning(pop)
#else
 #include <sqlext.h>
#endif

typedef struct {
    char                    *dsn;
    char                    *username;
    char                    *password;
    switch_memory_pool_t    *pool;
    JSContext               *ctx;
    switch_odbc_handle_t    *db;
    SQLCHAR                 *colbuf;
    uint32_t                colbufsz;
    SQLHSTMT                stmt;
} js_odbc_t;
JSClassID js_odbc_get_classid(JSContext *ctx);
switch_status_t js_odbc_class_register(JSContext *ctx, JSValue global_obj);
#endif // JS_ODBC_ENABLE

#endif
