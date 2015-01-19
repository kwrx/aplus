#include "stdio.h"


int printf(const char* fmt, ...) {
	
	va_list vargs;
	va_start(vargs, fmt);

	int r = vfprintf(stdout, fmt, vargs);

	va_end(vargs);
	return r;
} 
