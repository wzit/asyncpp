libname=asyncpp
CFLAGS=-static-libstdc++
release-static:
	g++ ${CFLAGS} -std=c++11 -O3 -c -DNDEBUG -Wall *.c *.cpp
	ar rcs lib${libname}.a *.o
	#ranlib lib${libname}.a
	rm *.o
debug-static:
	g++ ${CFLAGS} -std=c++11 -O1 -g -D_DEBUG -D_ASYNCPP_DEBUG -c -Wall *.c *.cpp	
	ar rcs lib${libname}d.a *.o
	#ranlib lib${libname}d.a
	rm *.o
release-shared:
	g++ ${CFLAGS} -std=c++11 -O3 -shared -fPIC -DNDEBUG -Wall *.c *.cpp -olib${libname}.so
debug-shared:
	g++ ${CFLAGS} -std=c++11 -O1 -g -shared -fPIC -D_DEBUG -D_ASYNCPP_DEBUG -Wall *.c *.cpp -olib${libname}d.so	
debug: debug-static debug-shared
release: release-static release-shared
all: debug release
