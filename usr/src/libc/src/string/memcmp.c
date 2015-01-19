#include "string.h"

int memcmp(const void* p1, const void* p2, size_t num) {
	const char* s1 = (const char*) p1;
	const char* s2 = (const char*) p2;

	while(num--)
		if(unlikely(*s1++ != *s2++))
			return (*--s1 > *--s2) ? 1 : -1;

	return 0;
}
