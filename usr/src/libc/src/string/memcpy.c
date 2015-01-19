#include "string.h"

void* memcpy(void* d, const void* s, size_t size) {
#if defined(__i386__) || defined(__x86_64__)

#if HAVE_SSE
	register int i;
	for(i = 0; (i + 16) < size; i += 16)
	__asm__ __volatile__ (
		"movups xmm0, [eax]\n"
		"movntdq [ebx], xmm0\n"
		: : "a"(s + i), "b"(d + i)
		: "memory"
	);

	__asm__ __volatile__ ("cld; rep movsb" : : "c"(size - i), "D"(d + i), "S"(s + i) : "memory");
#else
	__asm__ __volatile__ ("cld; rep movsb" : : "c"(size), "D"(d), "S"(s));
#endif

#else
	char* cd = (char*) d;
	char* cs = (char*) s;

	while(size--)
		*cd++ = *cs++;
#endif

	return d;
}
