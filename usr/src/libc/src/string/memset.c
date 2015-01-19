#include "string.h"

void* memset(void* d, int s, size_t size) {
#if defined(__i386__) || defined(__x86_64__)
	__asm__ __volatile__ ("cld; rep stosb" : : "c"(size), "D"(d), "a"(s) : "memory");
#else
	char* cd = (char*) d;
	while(size--)
		*cd++ = s;
#endif

	return d;
}
