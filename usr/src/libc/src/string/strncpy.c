#include "string.h"

char* strncpy(char* d, const char* s, size_t size) {
	register char* p = d;
	
	while(*s && size--)
		*d++ = *s++;

	return p;
}
