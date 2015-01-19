#include "stdlib.h"

extern void (*__libc_atexit_handler) (void);

void exit(int status) {
	if(likely(__libc_atexit_handler))
		__libc_atexit_handler();

	__libc_stdio_exit();
	_exit(status);
}
