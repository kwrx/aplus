#include "stdio.h"

int puts(const char* str) {
	if(unlikely(fputs(str, stdout) == EOF))
		return EOF;

	if(unlikely(fputc('\n', stdout) == EOF))
		return EOF;

	return 0;
}
