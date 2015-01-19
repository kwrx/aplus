#include "string.h"

char* strtok(char* s, const char* delim) {
	register char *spanp;
	register int c, sc;
	char *tok;
	static char *last;


	if (s == NULL && (s = last) == NULL)
		return (NULL);

cont:
	c = *s++;
	for(spanp = (char*)delim; (sc = *spanp++) != 0;) {
		if(c == sc)
			goto cont;
	}

	if (c == 0) {
		last = NULL;
		return (NULL);
	}
	tok = s - 1;

	
	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			if ((sc = *spanp++) == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				last = s;
				return (tok);
			}
		} while (sc != 0);
	}
}
