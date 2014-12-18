#include <atk.h>
#include <stdint.h>
#include <assert.h>

#include "config.h"

#ifdef HAVE_UNISTD_IO

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#ifndef O_BINARY
#define O_BINARY	0
#endif

Sint64 __rw_size(SDL_RWops* rw) {
	int sz = 0, cc = 0, fd = (int) rw->hidden.unknown.data1;

	cc = lseek(fd, 0, SEEK_CUR);
	lseek(fd, 0, SEEK_END);
	sz = lseek(fd, 0, SEEK_CUR);
	lseek(fd, cc, SEEK_SET);

	return (Sint64) sz;
}

Sint64 __rw_seek(SDL_RWops* rw, Sint64 offset, int whence) {

	switch(whence) {
		case RW_SEEK_SET:
			whence = SEEK_SET;
			break;
		case RW_SEEK_CUR:
			whence = SEEK_CUR;
			break;
		case RW_SEEK_END:
			whence = SEEK_END;
			break;
		default:
			ATK_ERROR("Invalid whence for file seek");
	}

	return (Sint64) lseek((int) rw->hidden.unknown.data1, (off_t) offset, whence);
}


size_t __rw_read(SDL_RWops* rw, void* data, size_t size, size_t count) {
	return (size_t) read((int) rw->hidden.unknown.data1, data, size * count);
}

size_t __rw_write(SDL_RWops* rw, const void* data, size_t size, size_t count) {
	return (size_t) write((int) rw->hidden.unknown.data1, (void*) data, size * count);
}

int __rw_close(SDL_RWops* rw) {
	return (int) close((int) rw->hidden.unknown.data1);
}


SDL_RWops* __RW_FromUnistd(const char* path, int flags, int mode) {
	int fd = open(path, flags | O_BINARY, mode);
	if(fd < 0)
		ATK_FILE_NOT_FOUND_N();

	SDL_RWops* rw = SDL_AllocRW();
	if(rw == NULL)
		ATK_OUT_OF_MEMORY_N();

	rw->size = __rw_size;
	rw->seek = __rw_seek;
	rw->read = __rw_read;
	rw->write = __rw_write;
	rw->close = __rw_close;
	rw->type = 0x78925306;
	rw->hidden.unknown.data1 = (void*) fd;

	return rw;
}

#endif


