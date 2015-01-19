#include "string.h"

char* strchr(const char* s, int v) {
	if(unlikely(!s))
		return NULL;

	while(*s)
		if(*s == v)
			return (char*) s;
		else
			s++;

	return NULL;
}
