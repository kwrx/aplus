#include "string.h"

char* strncat(char* d, const char* s, size_t size) {
	register char* p = d;

	while(*d) 
		d++;

	while(*s && size--)
		*d++ = *s++;

	*d = '\0';
	return p;
}
