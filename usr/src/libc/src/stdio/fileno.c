#include "stdio.h"


int fileno(FILE* fp) {
	return STDIO_FILE(fp)->fd;
}
