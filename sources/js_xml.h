/**
 * (C)2021 aks
 * https://github.com/akscf/
 **/
#ifndef JS_XML_H
#define JS_XML_H
#include "mod_quickjs.h"

typedef struct {
    uint8_t                 fl_free_xml;
    switch_xml_t            xml;
} js_xml_t;

JSClassID js_xml_get_classid(JSContext *ctx);
switch_status_t js_xml_class_register(JSContext *ctx, JSValue global_obj);

#endif


