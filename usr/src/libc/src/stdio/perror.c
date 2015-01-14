#include "stdio.h"

void perror(const char* str) {
	fputs(strerror(errno), stderr);
	fputs(": ", stderr);
	fputs(str, stderr);
	fputc('\n', stderr);
}
