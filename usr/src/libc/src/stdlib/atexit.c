#include "stdlib.h"


void (*__atexit_handler) (void) = NULL;

int atexit(void (*f) (void)) {
	__atexit_handler = f;
}

