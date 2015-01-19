#include "string.h"

void* memchr(const void* d, int val, size_t size) {
	char* p = (char*) d;

	while(size--)
		if(unlikely(*p == val))
			return (void*) p;
		else
			p++;

	return NULL;
}
