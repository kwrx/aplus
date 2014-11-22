#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char** argv) {

	printf("dlopen: %d (%s)\n", dlopen("/opt/cross/usr/src/libdl/test/lib.so", 0), strerror(errno));
	return 0;
}
