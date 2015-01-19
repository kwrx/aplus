#include "stdio.h"


int vprintf(const char* fmt, __VALIST vargs) {
	return vfprintf(stdout, fmt, vargs);
} 
