#include "string.h"

char* strrchr(const char* s, int v) {
	if(unlikely(!s))
		return NULL;

	int i = strlen(s) - 1;
	for(; i >= 0; i--)
		if(s[i] == v)
			return (char*) &s[i];

	return NULL;
}
