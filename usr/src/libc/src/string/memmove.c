#include "stdlib.h"

void* memmove(void* _d, const void* _s, size_t size) {
	char* d = _d;
	const char* s = _s;
	
	if(unlikely(d == s))
		return d;

	if(d > s)
		while(size--)
			d[size] = s[size];			
	else
		while(size--)
			*d++ = *s++;

	return _d;
}
