#include "stdio.h"


int sprintf(char* buf, const char* fmt, ...) {
	
	va_list vargs;
	va_start(vargs, fmt);

	int r = vsprintf(buf, fmt, vargs);

	va_end(vargs);
	return r;
} 
