#include "stdio.h"


int vscanf(const char* fmt, __VALIST vargs) {
	return vfscanf(stdin, fmt, vargs);
} 
