#include "stdio.h"

int putc(int ch, FILE* fp) {
	return fputc(ch, fp);
}
