#include "stdio.h"

FILE* freopen(const char* filename, const char* mode, FILE* fp) {
	FILE* fx = fopen(filename, mode);
	if(unlikely(!fx))
		return NULL;

	fclose(fp);

	STDIO_LOCK(fp);
	memcpy(fp, fx, sizeof(__STDIO_FILE));
	STDIO_UNLOCK(fp);

	return fp;
}
