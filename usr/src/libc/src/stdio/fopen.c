#include "stdio.h"


/* Required syscalls: 	*
 * -> open				*
 * -> close				*
 * -> read				*
 * -> write				*
 * -> lseek				*
*/


static size_t __fp_read(__STDIO_FILE* fp, void* buf, size_t size) {
	return read(fp->fd, buf, size);
}

static size_t __fp_write(__STDIO_FILE* fp, const void* buf, size_t size) {
	return write(fp->fd, buf, size);
}

static off_t __fp_seek(__STDIO_FILE* fp, off_t offset, int whence) {
	return lseek(fp->fd, offset, whence);
}

static int __fp_close(__STDIO_FILE* fp) {
	return close(fp->fd);
}

FILE* fopen(const char* name, const char* mode) {
	if(unlikely(!name || !mode)) {
		errno = EINVAL;
		return NULL;
	}


	__STDIO_FILE* fp = (__STDIO_FILE*) calloc(sizeof(__STDIO_FILE), 1);
	if(unlikely(!fp)) {
		errno = ENOMEM;
		return NULL;
	}

	while(*mode) {
		switch(*mode) {
			case 'r':
				fp->flags |= O_RDONLY;
				break;
			case 'w':
				fp->flags |= O_WRONLY | O_TRUNC;
				break;
			case 'a':
				fp->flags |= O_WRONLY | O_APPEND;
				break;
			case 'b':
				fp->flags |= O_BINARY;
				break;
			case '+':
				fp->flags |= O_RDWR;
				break;
			default:
				errno = EINVAL;
				free(fp);
				return NULL;
		}

		mode++;
	}

	fp->fd = open(name, fp->flags, 0644);
	if(fp->fd < 0)
		return NULL;

	fp->eof = 0;
	fp->error = 0;
	fp->data = NULL;
	STDIO_INIT_LOCK(fp);

	if((fp->flags & O_RDONLY) || (fp->flags & O_RDWR))
		fp->read = __fp_read;

	if((fp->flags & O_WRONLY) || (fp->flags & O_RDWR))
		fp->write = __fp_write;

	fp->seek = __fp_seek;
	fp->close = __fp_close;

	return (FILE*) fp;
}
