#include "string.h"

char* strpbrk(const char* s1, const char* s2) {
	register int i, j;
	for(i = 0; s1[i]; i++)
		for(j = 0; s2[j]; j++)
			if(s1[i] == s2[j])
				return (char*) &s1[i];

	return NULL;
}
