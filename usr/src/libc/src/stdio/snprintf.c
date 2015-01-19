#include "stdio.h"


int snprintf(char* buf, size_t size, const char* fmt, ...) {
	
	va_list vargs;
	va_start(vargs, fmt);

	int r = vsnprintf(buf, size, fmt, vargs);

	va_end(vargs);
	return r;
} 
