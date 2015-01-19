#include "string.h"

char* strcat(char* d, const char* s) {
	register char* p = d;

	while(*d) 
		d++;

	while(*s)
		*d++ = *s++;

	*d = '\0';
	return p;
}
