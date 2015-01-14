#include "stdio.h"

long int ftell(FILE* fp) {
	if(unlikely(!STDIO_FILE(fp)->seek))
		STDIO_ERROR(fp, EINVAL);
	
	STDIO_LOCK(fp);
	off_t r = STDIO_FILE(fp)->seek(STDIO_FILE(fp), 0, SEEK_CUR);
	STDIO_UNLOCK(fp);

	if(unlikely((int) r < 0))
		return EOF;

	return (long int) r;
}
