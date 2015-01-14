#include "stdio.h"

int fseek(FILE* fp, long int offset, int origin) {
	if(unlikely(!STDIO_FILE(fp)->seek))
		STDIO_ERROR(fp, EINVAL);
	
	STDIO_LOCK(fp);
	off_t r = STDIO_FILE(fp)->seek(STDIO_FILE(fp), (off_t) offset, origin);
	STDIO_UNLOCK(fp);

	if(unlikely((int) r < 0))
		return EOF;

	return 0;
}
