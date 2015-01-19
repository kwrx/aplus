#include "stdlib.h"


void (*__libc_at_quick_exit_handler) (void) = NULL;

int at_quick_exit(void (*f) (void)) {
	__libc_at_quick_exit_handler = f;
}

