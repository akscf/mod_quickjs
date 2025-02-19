include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_quickjs

mod_LTLIBRARIES = mod_quickjs.la
mod_quickjs_la_SOURCES  = mod_quickjs.c utils.c curl_hlp.c js_session.c js_session_misc.c js_codec.c js_dtmf.c js_event.c js_filehandle.c js_file.c js_socket.c js_coredb.c js_eventhandler.c js_curl.c js_curl_misc.c js_odbc.c js_xml.c
mod_quickjs_la_CFLAGS   = $(AM_CFLAGS) -I/opt/quickjs/include/quickjs -I. -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-unused-label -Wno-declaration-after-statement -Wno-pedantic
mod_quickjs_la_LIBADD   = $(switch_builddir)/libfreeswitch.la /opt/quickjs/lib/quickjs/libquickjs.lto.a
#mod_quickjs_la_LIBADD   = $(switch_builddir)/libfreeswitch.la /opt/quickjs/lib/libquickjs.la
mod_quickjs_la_LDFLAGS  = -avoid-version -module -no-undefined -shared

$(am_mod_quickjs_la_OBJECTS):  mod_quickjs.h

