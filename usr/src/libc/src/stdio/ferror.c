#include "stdio.h"

int ferror(FILE* fp) {
	return STDIO_FILE(fp)->error;
}
