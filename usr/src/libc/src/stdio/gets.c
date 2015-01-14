#include "stdio.h"

char* gets(char* buf) {
	return fgets(buf, BUFSIZ, stdin);
}
