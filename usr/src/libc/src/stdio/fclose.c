#include "stdio.h"

int fclose(FILE* fp) {
	STDIO_LOCK(fp);
	
	if(likely(STDIO_FILE(fp)->close))
		if(unlikely(STDIO_FILE(fp)->close(STDIO_FILE(fp)) != 0))
			return EOF;

	
	
	STDIO_FILE(fp)->fd = -1;
	STDIO_FILE(fp)->data = NULL;

	STDIO_UNLOCK(fp);

	free(fp);	
	return 0;
}
