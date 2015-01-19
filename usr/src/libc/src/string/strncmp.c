#include "string.h"

/* FIXME */
int _strncmp (const char* s1, const char* s2, size_t size) {
	
	while(size--)
		if(*s1 != *s2)
	    	return ((*(unsigned char *) s1 < *(unsigned char *) s2) ? -1 : 1);
		else if (*s1 == '\0')
	    	return 0;

    return 0;
}
