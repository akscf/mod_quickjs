all: fib.c
	gcc -c -Wall -Werror -fpic -I/opt/quickjs/include/quickjs fib.c
	gcc -shared -o fib.so fib.o -L/opt/quickjs/lib/quickjs/ -lquickjs.lto
