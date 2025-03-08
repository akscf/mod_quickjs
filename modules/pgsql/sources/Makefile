QJS_INC=-I/opt/quickjs/include/quickjs
QJS_LIB=-L/opt/quickjs/lib/quickjs/ -lquickjs.lto

FS_INC=-I/opt/freeswitch/include/freeswitch
FS_LIB=-L/opt/freeswitch/lib -lfreeswitch

PG_INC=-I$(shell pg_config --includedir)
PG_LIB=-L$(shell pg_config --libdir) -lpq


# mod_quickjs
CFLAGS=-Wall -Werror -fpic -DQJS_FREESWITCH -I. ${QJS_INC} ${FS_INC} ${PG_INC}
LIBS=${QJS_LIB} ${FS_LIB} ${PG_LIB}

# pure qjs
#CFLAGS=-Wall -Werror -fpic -I. ${QJS_INC} ${PG_INC}
#LIBS=${QJS_LIB} ${PG_LIB}


all: pgsql.c
	gcc -c ${CFLAGS} pgsql.c
	gcc -shared -o pgsql.so pgsql.o ${LIBS}

clean:
	rm -f pgsql.so pgsql.o
    
