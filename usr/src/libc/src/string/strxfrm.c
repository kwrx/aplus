#include "string.h"

size_t strxfrm(char* d, const char* s, size_t size) {
	if(likely(d))
		strncpy(d, s, size);
	else
		return strlen(s);

	return size;
}
