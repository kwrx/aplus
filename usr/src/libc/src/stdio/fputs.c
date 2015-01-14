#include "stdio.h"

int fputs(const char* str, FILE* fp) {
	if(unlikely(!str))
		STDIO_ERROR(fp, EINVAL);

	while(*str)
		if(fputc(*str++, fp) == EOF)
			return EOF;

	return 0;
}
