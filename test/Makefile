all: cbinder_add_ints

cbinder_add_ints: cbinder_add_ints.c
	g++ -static -std=c++11 -o middle.o -I../libs/include -I.. -L../libs -DHAVE_PTHREADS -DHAVE_SYS_UIO_H -DHAVE_ENDIAN_H -DHAVE_ANDROID_OS=1 -c middle.cpp -lpthread -lbinder
	g++ -std=c++11 -o $@ -I../libs/include -I.. -L../libs -DHAVE_PTHREADS -DHAVE_SYS_UIO_H -DHAVE_ENDIAN_H -DHAVE_ANDROID_OS=1 $< middle.o testUtil.c -lpthread -lbinder
	#gcc -o $@ $< middle.o -I../libs/include -I.. -L../libs -DHAVE_PTHREADS -DHAVE_SYS_UIO_H -DHAVE_ENDIAN_H -DHAVE_ANDROID_OS=1 -lpthread -lbinder
 

clean:
	rm -f *.o cbinder_add_ints
