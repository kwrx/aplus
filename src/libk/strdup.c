#include <xdev.h>
#include <xdev/mm.h>
#include <libc.h>

char* strdup(const char* s) {
	char* p = (char*) kmalloc(strlen(s) + 1, GFP_KERNEL);
	strcpy(p, s);

	return p;
}

EXPORT(strdup);
