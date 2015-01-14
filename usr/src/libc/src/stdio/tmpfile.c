#include "stdio.h"

/* Required syscalls	*
 * -> open				*
 * -> close				*
 */

FILE* tmpfile(void) {
	char buf[L_tmpnam];
	char* p = tmpnam(buf);
	
	int fd = open(p, O_CREAT | O_EXCL | O_TRUNC | O_BINARY, 0644);
	if(fd < 0)
		return NULL;

	FILE* fp = fdopen(fd, "wb+");
	if(!fp) {
		close(fd);
		return NULL;
	}

	return fp;
}
