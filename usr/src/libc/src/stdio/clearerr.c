#include "stdio.h"

void clearerr(FILE* fp) {
	STDIO_FILE(fp)->error = 0;
}
