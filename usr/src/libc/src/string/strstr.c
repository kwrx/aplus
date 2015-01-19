#include "string.h"


char* strstr(const char* s1, const char* s2) {
	char c;
	size_t len;

	c = *s2++;
	if(!c)
		return (char *) s1;

	len = strlen(s1);
	do {
		char sc;
		do {
			sc = *s1++;
			if (!sc)
	    		return (char *) 0;
        } while (sc != c);
    } while (strncmp(s1, s2, len) != 0);

    return (char *) (s1 - 1);
}
