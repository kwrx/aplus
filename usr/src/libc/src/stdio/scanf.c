#include "stdio.h"


int scanf(const char* fmt, ...) {
	
	va_list vargs;
	va_start(vargs, fmt);

	int r = vfscanf(stdin, fmt, vargs);

	va_end(vargs);
	return r;
} 
