#include "stdio.h"

int fputc(int ch, FILE* fp) {
	if(unlikely(fwrite(&ch, 1, 1, fp) < 1))
		return EOF;

	return ch;
}
