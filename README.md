Lightweight javascript engine for Freeswitch

----<br>

Brief guide to build and install the module:<br>

1) Download and build quickjs (see details on: https://bellard.org/quickjs/)<br>
   - download archive: https://bellard.org/quickjs/quickjs-2021-03-27.tar.xz
   - #tar -xpf quickjs-2021-03-27.tar.xz
   - #cd quickjs-2021-03-27
   - #make prefix=/opt/quickjs clean all install

2) Build mod_quickjs<br>
   I hope you have a necessary experience in libtool and you know how to build Freeswitch modules from the sources (otherwise you need to read documentations about these topics).<br>
   This part will describe the aspects which concern only libquickjs.<br>

   - Library path (see Makefile.am)
      by default libquickjs path points to: /opt/quickjs/lib/quickjs/<br>

   - Type of quickjs library (static/dynamic)<br>
      by default mod_quickjs references to a static version of libquickjs<br>
      mod_quickjs_la_LIBADD   = $(switch_builddir)/libfreeswitch.la /opt/quickjs/lib/quickjs/libquickjs.lto.a<br>

      If you built it as dynamic you should comment up the line above and uncomment one below:<br>
      #mod_quickjs_la_LIBADD   = $(switch_builddir)/libfreeswitch.la /opt/quickjs/lib/libquickjs.la<br>

      (see Makefile.am it looks like more understandable there)

   - and don't foreger to regenerate Makefile.in after all the changes (run 'automake' from the Freeswitch source root path)<br>

3) example scripts<br>
   https://github.com/akscf/mod_quickjs/tree/main/sources<br>


----<br>

todo:<br>
 write more documentations (classes, api, and so on)....<br>
