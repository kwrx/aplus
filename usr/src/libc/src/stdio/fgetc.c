#include "stdio.h"

int fgetc(FILE* fp) {
	int ch;
	if(unlikely(fread(&ch, 1, 1, fp) < 1))
		return EOF;

	return ch;
}
