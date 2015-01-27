libname=asyncpp
CFLAGS=
debug-static:
	g++ ${CFLAGS} -std=c++11 -O1 -g -c -Wall *.c *.cpp	
	ar rc lib${libname}d.a *.o
	ranlib lib${libname}d.a
	rm *.o
release-static:
	g++ ${CFLAGS} -std=c++11 -O2 -c -Wall *.c *.cpp
	ar rc lib${libname}.a *.o
	ranlib lib${libname}.a
	rm *.o
debug-shared:
	g++ ${CFLAGS} -std=c++11 -O1 -g -shared -fPIC -Wall *.c *.cpp -olib${libname}d.so	
release-shared:
	g++ ${CFLAGS} -std=c++11 -O2 -shared -fPIC -Wall *.c *.cpp -olib${libname}.so
debug: debug-static debug-shared
release: release-static release-shared
all: debug release
