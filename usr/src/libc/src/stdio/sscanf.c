#include "stdio.h"


int sscanf(const char* buf, const char* fmt, ...) {
	
	va_list vargs;
	va_start(vargs, fmt);

	int r = vsscanf(buf, fmt, vargs);

	va_end(vargs);
	return r;
} 
