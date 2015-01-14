#include "stdio.h"


int ungetc(int ch, FILE* fp) { /* FIXME */
	if(unlikely(ch == EOF))
		return EOF;

	off_t pos = ftell(fp);
	if(unlikely(pos == 0))
		return EOF;

	if(unlikely(fseek(fp, -1, SEEK_CUR) < 0))
		return EOF;

	STDIO_FILE(fp)->eof = 0;

	if(unlikely(fputc(ch, fp) == EOF))
		return EOF;
	
	if(unlikely(fseek(fp, -1, SEEK_CUR) < 0))
		return EOF;

	STDIO_FILE(fp)->eof = 0;

	return ch;
}
