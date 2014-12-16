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

#endif



int atk_load_bitmap(atk_t* atk, atk_bitmap_t* bitmap, const char* path) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!bitmap)
		ATK_ERROR("Invalid Bitmap context");

	if(!path)
		ATK_ERROR("Path cannot be null");


#if HAVE_UNISTD_IO

	int fd = open(path, O_RDONLY | O_BINARY, 0644);
	if(fd < 0)
		ATK_FILE_NOT_FOUND();

	SDL_RWops* rw = SDL_AllocRW();
	if(rw == NULL)
		ATK_OUT_OF_MEMORY();

	rw->size = __rw_size;
	rw->seek = __rw_seek;
	rw->read = __rw_read;
	rw->write = __rw_write;
	rw->close = __rw_close;
	rw->type = 0x78925306;
	rw->hidden.unknown.data1 = fd;
#else
	SDL_RWops* rw = SDL_RWFromFile(path, "rb");
#endif

	bitmap = (atk_bitmap_t*) SDL_LoadBMP_RW(rw, 1);
	bitmap = (atk_bitmap_t*) SDL_ConvertSurface((SDL_Surface*) bitmap, atk->surface->format, NULL);

	if(bitmap == NULL)
		ATK_ERROR("Loading and convert bitmap failed");	

	return 0;
}

int atk_save_bitmap(atk_t* atk, atk_bitmap_t* bitmap, const char* path) {
	if(!atk)
		ATK_ERROR("Invalid ATK context");

	if(!bitmap)
		ATK_ERROR("Invalid Bitmap context");

	if(!path)
		ATK_ERROR("Path cannot be null");

#if HAVE_UNISTD_IO

	int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644);
	if(fd < 0)
		ATK_FILE_NOT_FOUND();

	SDL_RWops* rw = SDL_AllocRW();
	if(rw == NULL)
		ATK_OUT_OF_MEMORY();

	rw->size = __rw_size;
	rw->seek = __rw_seek;
	rw->read = __rw_read;
	rw->write = __rw_write;
	rw->close = __rw_close;
	rw->type = 0x78925306;
	rw->hidden.unknown.data1 = fd;
#else
	SDL_RWops* rw = SDL_RWFromFile(path, "wb");
#endif

	return SDL_SaveBMP_RW((SDL_Surface*) bitmap, rw, 1);
}

