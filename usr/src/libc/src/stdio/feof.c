#include "stdio.h"

int feof(FILE* fp) {
	return STDIO_FILE(fp)->eof;
}
