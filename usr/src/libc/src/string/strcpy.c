#include "string.h"

char* strcpy(char* d, const char* s) {
	register char* p = d;
	while(*s)
		*d++ = *s++;

	return p;
}
