#include "string.h"

size_t strspn(const char* s1, const char* s2) {
	register int i, j, z = 0;
	for(i = 0; s1[i]; i++)
		for(j = 0; s2[j]; j++)
			if(s1[i] == s2[j])
				z++;

	return z;
}
