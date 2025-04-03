/**
 **
 ** (C)2025 aks
 **/
#include <pgsql.h>

#define CLASS_NAME          "Pgsql"
#define PROP_DSN            1
#define PROP_CONNECTED      2
#define PROP_ERROR          3

#define PGSQL_SANITY_CHECK() if (!js_pgsql) { return JS_ThrowTypeError(ctx, "Reference corrupted (js_pgsql == null)"); }
#define CONN_SANITY_CHECK()  if (!js_pgsql->conn || !js_pgsql->fl_conntected) { return JS_ThrowTypeError(ctx, "No database connection"); }

static JSClassID js_pgsql_class_id;
static void js_pgsql_finalizer(JSRuntime *rt, JSValue val);
static JSValue js_pgsql_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv);

static const char* field_type(PGresult* res, int field) {
    const char *type = 0;
    switch(PQftype(res, field)) {
        case 16: type = "bool"; break;
        case 17: type = "bytea"; break;
        case 18: type = "char"; break;
        case 19: type = "name"; break;
        case 20: type = "int8"; break;
        case 21: type = "int2"; break;
        case 22: type = "int2vector"; break;
        case 23: type = "int4"; break;
        case 24: type = "regproc"; break;
        case 25: type = "text"; break;
        case 26: type = "oid"; break;
        case 27: type = "tid"; break;
        case 28: type = "xid"; break;
        case 29: type = "cid"; break;
        case 30: type = "oidvector"; break;
        case 114: type = "json"; break;
        case 142: type = "xml"; break;
        case 194: type = "pgnodetree"; break;
        case 3361: type = "pgndistinct"; break;
        case 3402: type = "pgdependencies"; break;
        case 5017: type = "pgmcvlist"; break;
        case 32: type = "pgddlcommand"; break;
        case 600: type = "point"; break;
        case 601: type = "lseg"; break;
        case 602: type = "path"; break;
        case 603: type = "box"; break;
        case 604: type = "polygon"; break;
        case 628: type = "line"; break;
        case 700: type = "float4"; break;
        case 701: type = "float8"; break;
        case 705: type = "unknown"; break;
        case 718: type = "circle"; break;
        case 790: type = "cash"; break;
        case 829: type = "macaddr"; break;
        case 869: type = "inet"; break;
        case 650: type = "cidr"; break;
        case 774: type = "macaddr8"; break;
        case 1033: type = "aclitem"; break;
        case 1042: type = "bpchar"; break;
        case 1043: type = "varchar"; break;
        case 1082: type = "date"; break;
        case 1083: type = "time"; break;
        case 1114: type = "timestamp"; break;
        case 1184: type = "timestamptz"; break;
        case 1186: type = "interval"; break;
        case 1266: type = "timetz"; break;
        case 1560: type = "bit"; break;
        case 1562: type = "varbit"; break;
        case 1700: type = "numeric"; break;
        case 1790: type = "refcursor"; break;
        case 2202: type = "regprocedure"; break;
        case 2203: type = "regoper"; break;
        case 2204: type = "regoperator"; break;
        case 2205: type = "regclass"; break;
        case 2206: type = "regtype"; break;
        case 4096: type = "regrole"; break;
        case 4089: type = "regnamespace"; break;
        case 2950: type = "uuid"; break;
        case 3220: type = "lsn"; break;
        case 3614: type = "tsvector"; break;
        case 3642: type = "gtsvector"; break;
        case 3615: type = "tsquery"; break;
        case 3734: type = "regconfig"; break;
        case 3769: type = "regdictionary"; break;
        case 3802: type = "jsonb"; break;
        case 4072: type = "jsonpath"; break;
        case 2970: type = "txid_snapshot"; break;
        case 3904: type = "int4range"; break;
        case 3906: type = "numrange"; break;
        case 3908: type = "tsrange"; break;
        case 3910: type = "tstzrange"; break;
        case 3912: type = "daterange"; break;
        case 3926: type = "int8range"; break;
        case 2249: type = "record"; break;
        case 2287: type = "recordarray"; break;
        case 2275: type = "cstring"; break;
        case 2276: type = "any"; break;
        case 2277: type = "anyarray"; break;
        case 2278: type = "void"; break;
        case 2279: type = "trigger"; break;
        case 3838: type = "evttrigger"; break;
        case 2280: type = "language_handler"; break;
        case 2281: type = "internal"; break;
        case 2282: type = "opaque"; break;
        case 2283: type = "anyelement"; break;
        case 2776: type = "anynonarray"; break;
        case 3500: type = "anyenum"; break;
        case 3115: type = "fdw_handler"; break;
        case 325: type = "index_am_handler"; break;
        case 3310: type = "tsm_handler"; break;
        case 269: type = "table_am_handler"; break;
        case 3831: type = "anyrange"; break;
        case 1000: type = "boolarray"; break;
        case 1001: type = "byteaarray"; break;
        case 1002: type = "chararray"; break;
        case 1003: type = "namearray"; break;
        case 1016: type = "int8array"; break;
        case 1005: type = "int2array"; break;
        case 1006: type = "int2vectorarray"; break;
        case 1007: type = "int4array"; break;
        case 1008: type = "regprocarray"; break;
        case 1009: type = "textarray"; break;
        case 1028: type = "oidarray"; break;
        case 1010: type = "tidarray"; break;
        case 1011: type = "xidarray"; break;
        case 1012: type = "cidarray"; break;
        case 1013: type = "oidvectorarray"; break;
        case 199: type = "jsonarray"; break;
        case 143: type = "xmlarray"; break;
        case 1017: type = "pointarray"; break;
        case 1018: type = "lsegarray"; break;
        case 1019: type = "patharray"; break;
        case 1020: type = "boxarray"; break;
        case 1027: type = "polygonarray"; break;
        case 629: type = "linearray"; break;
        case 1021: type = "float4array"; break;
        case 1022: type = "float8array"; break;
        case 719: type = "circlearray"; break;
        case 791: type = "moneyarray"; break;
        case 1040: type = "macaddrarray"; break;
        case 1041: type = "inetarray"; break;
        case 651: type = "cidrarray"; break;
        case 775: type = "macaddr8array"; break;
        case 1034: type = "aclitemarray"; break;
        case 1014: type = "bpchararray"; break;
        case 1015: type = "varchararray"; break;
        case 1182: type = "datearray"; break;
        case 1183: type = "timearray"; break;
        case 1115: type = "timestamparray"; break;
        case 1185: type = "timestamptzarray"; break;
        case 1187: type = "intervalarray"; break;
        case 1270: type = "timetzarray"; break;
        case 1561: type = "bitarray"; break;
        case 1563: type = "varbitarray"; break;
        case 1231: type = "numericarray"; break;
        case 2201: type = "refcursorarray"; break;
        case 2207: type = "regprocedurearray"; break;
        case 2208: type = "regoperarray"; break;
        case 2209: type = "regoperatorarray"; break;
        case 2210: type = "regclassarray"; break;
        case 2211: type = "regtypearray"; break;
        case 4097: type = "regrolearray"; break;
        case 4090: type = "regnamespacearray"; break;
        case 2951: type = "uuidarray"; break;
        case 3221: type = "pg_lsnarray"; break;
        case 3643: type = "tsvectorarray"; break;
        case 3644: type = "gtsvectorarray"; break;
        case 3645: type = "tsqueryarray"; break;
        case 3735: type = "regconfigarray"; break;
        case 3770: type = "regdictionaryarray"; break;
        case 3807: type = "jsonbarray"; break;
        case 4073: type = "jsonpatharray"; break;
        case 2949: type = "txid_snapshotarray"; break;
        case 3905: type = "int4rangearray"; break;
        case 3907: type = "numrangearray"; break;
        case 3909: type = "tsrangearray"; break;
        case 3911: type = "tstzrangearray"; break;
        case 3913: type = "daterangearray"; break;
        case 3927: type = "int8rangearray"; break;
        default: break;
    }

    return type;
}

static JSValue js_xxmap(JSContext *ctx, JSValue tmap, const char *tname, char *tvalue) {
    JSValue tte;
    JSValue result;

    if(zstr(tvalue)) {
        return JS_NULL;
    }

    tte = JS_GetPropertyStr(ctx, tmap, tname);
    if(!QJS_IS_NULL(tte)) {
        if(JS_IsFunction(ctx, tte)) {
            JSValue args[2] = { 0 };
            args[0] = JS_NewString(ctx, tname);
            args[1] = JS_NewString(ctx, tvalue);

            result = JS_Call(ctx, tte, JS_UNDEFINED, 2, (JSValueConst *)args);
            if(JS_IsException(result)) {
                JS_ResetUncatchableError(ctx);
            }

            JS_FreeValue(ctx, args[0]);
            JS_FreeValue(ctx, args[1]);
        } else {
            const char *ttname = JS_ToCString(ctx, tte);
            if(!zstr(ttname)) {
                if(!strcmp(ttname, "bool")) {
                    result = !strcmp(tvalue, "true") ? JS_TRUE : JS_FALSE;
                } else if(!strcmp(ttname, "int")) {
                    result = JS_NewInt32(ctx, atoi(tvalue));
                } else if(!strcmp(ttname, "float") || !strcmp(ttname, "double")) {
                    result = JS_NewFloat64(ctx, strtod(tvalue, NULL));
                } else {
                    result = JS_NewString(ctx, tvalue);
                }
            } else {
                result = JS_NewString(ctx, tvalue);
            }
            JS_FreeCString(ctx, ttname);
        }
    } else {
        result = JS_NewString(ctx, tvalue);
    }

    JS_FreeValue(ctx, tte);
    return result;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSValue js_pgsql_property_get(JSContext *ctx, JSValueConst this_val, int magic) {
    qjs_pgsql_t *js_pgsql = JS_GetOpaque2(ctx, this_val, js_pgsql_class_id);

    if(!js_pgsql) {
        return JS_UNDEFINED;
    }

    switch(magic) {
        case PROP_DSN: {
            return JS_NewString(ctx, js_pgsql->dsn);
        }
        case PROP_CONNECTED: {
            return js_pgsql->fl_conntected ? JS_TRUE : JS_FALSE;
        }
        case PROP_ERROR: {
            if(js_pgsql->conn) {
                char *p = PQerrorMessage(js_pgsql->conn);
                return p ? JS_NewString(ctx, p) : JS_UNDEFINED;
            }
            return JS_UNDEFINED;
        }
    }

    return JS_UNDEFINED;
}

static JSValue js_pgsql_property_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    qjs_pgsql_t *js_pgsql = JS_GetOpaque2(ctx, this_val, js_pgsql_class_id);

    if(!js_pgsql) {
        return JS_UNDEFINED;
    }

    return JS_FALSE;
}

// execQuery(query, [callback, callbackData])
static JSValue js_pgsql_exec_query(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    qjs_pgsql_t *js_pgsql = JS_GetOpaque2(ctx, this_val, js_pgsql_class_id);
    const char *query;
    PGresult  *res = NULL;
    JSValue result = JS_FALSE;

    PGSQL_SANITY_CHECK();
    CONN_SANITY_CHECK();

    if(!argc || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "execQuery(query, [callback, callbackData])");
    }

    query = JS_ToCString(ctx, argv[0]);
    if(zstr(query)) { return JS_FALSE; }

    if(argc > 1 && JS_IsFunction(ctx, argv[1])) {
        JSValue fnc = argv[1];
        JSValue usr_data = (argc > 2 ? argv[2] : JS_UNDEFINED);

        res = PQexec(js_pgsql->conn, query);
        if(PQresultStatus(res) == PGRES_TUPLES_OK) {
            int ncols = PQnfields(res);
            int nrows = PQntuples(res);
            int rr = 0;

            for(int i = 0; i < nrows; i++) {
                JSValue cb_ret;
                JSValue cb_args[2] = { 0 };
                JSValue row_data = JS_NewObject(ctx);

                for(int j = 0; j < ncols; j++) {
                    char *name = PQfname(res, j);
                    char *value = PQgetvalue(res, i, j);
                    const char *type = field_type(res, j);
                    JSValue mtype;

                    if(!JS_IsUndefined(js_pgsql->tmap)) {
                        mtype = js_xxmap(ctx, js_pgsql->tmap, type, value);
                    } else {
                        mtype = JS_NewString(ctx, value);
                    }

                    JSValue col_val = JS_NewObject(ctx);
                    JS_SetPropertyStr(ctx, col_val, "type", JS_NewString(ctx, type));
                    JS_SetPropertyStr(ctx, col_val, "value", mtype);
                    JS_SetPropertyStr(ctx, row_data, name, col_val);
                }

                cb_args[0] = row_data;
                cb_args[1] = usr_data;
                cb_ret = JS_Call(ctx, fnc, JS_UNDEFINED, 2, (JSValueConst *)cb_args);
                if(JS_IsException(cb_ret)) {
                    log_error("Unexpected exception (cb_ret)");
                    JS_ResetUncatchableError(ctx);
                    rr = 1;
                } else if(JS_IsBool(cb_ret)) {
                    rr = !JS_ToBool(ctx, cb_ret);
                }

                JS_FreeValue(ctx, row_data);
                JS_FreeValue(ctx, cb_ret);

                if(rr) { break; }
            }

            result = JS_TRUE;
        } else {
            log_error("Unexpected result type (res != PGRES_TUPLES_OK)");
        }
    } else {
        res = PQexec(js_pgsql->conn, query);
        if(PQresultStatus(res) == PGRES_COMMAND_OK || PQresultStatus(res) == PGRES_TUPLES_OK) {
            result = JS_TRUE;
        }
    }

    if(res) {
        PQclear(res);
    }

    JS_FreeCString(ctx, query);
    return result;
}

// --------------------------------------------------------------------------------------------------------------------------------------------------------------
static JSClassDef js_pgsql_class = {
    CLASS_NAME,
    .finalizer = js_pgsql_finalizer,
};

static const JSCFunctionListEntry js_pgsql_proto_funcs[] = {
    JS_CGETSET_MAGIC_DEF("dsn", js_pgsql_property_get, js_pgsql_property_set, PROP_DSN),
    JS_CGETSET_MAGIC_DEF("error", js_pgsql_property_get, js_pgsql_property_set, PROP_ERROR),
    JS_CGETSET_MAGIC_DEF("isConnected", js_pgsql_property_get, js_pgsql_property_set, PROP_CONNECTED),
    //
    JS_CFUNC_DEF("execQuery", 1, js_pgsql_exec_query),
};

static void js_pgsql_finalizer(JSRuntime *rt, JSValue val) {
    qjs_pgsql_t *js_pgsql = JS_GetOpaque(val, js_pgsql_class_id);

    if(!js_pgsql) {
        return;
    }

#ifdef QJS_PGSQL_DEBUG
    log_debug("qjs-pgsql-finalizer: js_pgsql=%p (conn=%p, dbh=%p)\n", js_pgsql, js_pgsql->conn, js_pgsql->dbh);
#endif

    if(js_pgsql->dbh) {
        switch_cache_db_handle_t *dbh = js_pgsql->dbh;
        switch_cache_db_release_db_handle(&dbh);
    } else {
        if(js_pgsql->conn) {
            PQfinish(js_pgsql->conn);
        }
    }

    if(js_pgsql->dsn) {
        js_free_rt(rt, js_pgsql->dsn);
    }

    js_free_rt(rt, js_pgsql);
}

// new Pgsql(dsn [, typeMmapping])
static JSValue js_pgsql_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    qjs_pgsql_t *js_pgsql = NULL;
    JSValue err = JS_UNDEFINED;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;
    const char *dsn = NULL;

    if(argc < 1 || QJS_IS_NULL(argv[0])) {
        return JS_ThrowTypeError(ctx, "Pgsql(dsn[, typeMaping])");
    }

    dsn = JS_ToCString(ctx, argv[0]);
    if(zstr(dsn)) {
        return JS_ThrowTypeError(ctx, "Inavalid arguament: dsn");
    }

    js_pgsql = js_mallocz(ctx, sizeof(qjs_pgsql_t));
    if(!js_pgsql) { goto fail;  }

    if(argc > 1 && !QJS_IS_NULL(argv[1]) && JS_IsObject(argv[1])) {
        js_pgsql->tmap = argv[1];
    } else {
        js_pgsql->tmap = JS_UNDEFINED;
    }

    if(strstr(dsn, "dbh:")) {
#ifdef QJS_DBH_ENABLED
        const char *p = dsn + 4;
        switch_cache_db_handle_t *dbh = NULL;

        js_pgsql->dsn = js_strdup(ctx, p);
        if(switch_cache_db_get_db_handle_dsn(&dbh, p) == SWITCH_STATUS_SUCCESS) {
            js_pgsql->conn = switch_cache_db_get_native_pg_conn(dbh);
            js_pgsql->dbh = dbh;
            js_pgsql->fl_conntected = 1;
        }
#else
        return JS_ThrowTypeError(ctx, "Unsupported");
#endif
    } else {
        js_pgsql->dsn = js_strdup(ctx, dsn);
        js_pgsql->conn = PQconnectdb(js_pgsql->dsn);
        if(PQstatus(js_pgsql->conn) == CONNECTION_OK) {
            js_pgsql->fl_conntected = 1;
        }
    }


    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if(JS_IsException(proto)) { goto fail; }

    obj = JS_NewObjectProtoClass(ctx, proto, js_pgsql_class_id);
    JS_FreeValue(ctx, proto);
    if(JS_IsException(obj)) { goto fail; }

    JS_SetOpaque(obj, js_pgsql);

#ifdef QJS_PGSQL_DEBUG
    log_debug("qjs-pgsql-constructor: js_pgsql=%p (conn=%p, dbh=%p)\n", js_pgsql, js_pgsql->conn, js_pgsql->dbh);
#endif

    JS_FreeCString(ctx, dsn);
    return obj;

fail:
    if(js_pgsql) {
        if(js_pgsql->conn) {
            PQfinish(js_pgsql->conn);
        }
        js_free(ctx, js_pgsql);
    }
    JS_FreeValue(ctx, obj);
    JS_FreeCString(ctx, dsn);
    return (JS_IsUndefined(err) ? JS_EXCEPTION : err);
}

static int js_pgsql_init(JSContext *ctx, JSModuleDef *m) {
    JSValue pgsql_proto, pgsql_class;

    JS_NewClassID(&js_pgsql_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_pgsql_class_id, &js_pgsql_class);

    pgsql_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, pgsql_proto, js_pgsql_proto_funcs, ARRAY_SIZE(js_pgsql_proto_funcs));

    pgsql_class = JS_NewCFunction2(ctx, js_pgsql_ctor, CLASS_NAME, 1, JS_CFUNC_constructor, 0);

    JS_SetConstructor(ctx, pgsql_class, pgsql_proto);
    JS_SetClassProto(ctx, js_pgsql_class_id, pgsql_proto);

    JS_SetModuleExport(ctx, m, CLASS_NAME, pgsql_class);
    return 0;
}

JSModuleDef *js_init_module(JSContext *ctx, const char *module_name) {
    JSModuleDef *m;

    m = JS_NewCModule(ctx, module_name, js_pgsql_init);
    if(!m) return NULL;

    JS_AddModuleExport(ctx, m, CLASS_NAME);
    return m;
}

