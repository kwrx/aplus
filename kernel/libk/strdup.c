#include <aplus.h>
#include <aplus/mm.h>
#include <libc.h>

char* strdup(const char* s) {
	char* p = (char*) kmalloc(strlen(s) + 1, GFP_KERNEL);
	strcpy(p, s);

	return p;
}

char* strndup(const char* s, size_t n) {

	size_t k;
	if(strlen(s) > n)
		k = n;
	else
		k = strlen(s);

	char* p = (char*) kmalloc(k + 1, GFP_KERNEL);
	strncpy(p, s, k);

	return p;
}

EXPORT(strdup);
EXPORT(strndup);
