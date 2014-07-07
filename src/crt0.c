
//
//  crt0.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is kfree software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.


#include <newlib.h>
#include <_ansi.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <errno.h>
#undef errno
int errno;


#define sc(n, b, c, d, e, f)																			\
	int r;																								\
	__asm__ __volatile__ ("int 0x80" : "=a"(r) : "a"(n), "b"(b), "c"(c), "d"(d), "S"(e), "D"(f));		\
	return r
	
#define sc_noret(n, b, c, d, e, f)																		\
	__asm__ __volatile__ ("int 0x80" : : "a"(n), "b"(b), "c"(c), "d"(d), "S"(e), "D"(f))





void _exit(int status) {
	sc_noret(0, status, 0, 0, 0, 0);
	for(;;);
}

int chown(const char* path, uid_t owner, gid_t group) {
	sc(1, path, owner, group, 0, 0);
}

int close(int fildes) {
	sc(2, fildes, 0, 0, 0, 0);
}

int execve(const char* name, char* const argv[], char* const env[]) {
	sc(3, name, argv, env, 0, 0);
}

int fork() {
	sc(4, 0, 0, 0, 0, 0);
}

int fstat(int fildes, struct stat* st) {
	sc(5, fildes, st, 0, 0, 0);
}

int getpid() {
	sc(6, 0, 0, 0, 0, 0);
}

int gettimeofday(struct timeval* ptimeval, void* ptimezone) {
	sc(7, ptimeval, ptimezone, 0, 0, 0);
}

int isatty(int file) {
	return file < 3;
}

int kill(int pid, int sig) {
	sc(8, pid, sig, 0, 0, 0);
}

int link(const char* existing, const char* new) {
	sc(9, existing, new, 0, 0, 0);
}

off_t lseek(int file, off_t ptr, int dir) {
	sc(10, file, ptr, dir, 0, 0);
}

int open(const char* file, int flags, int mode) {
	sc(11, file, flags, mode, 0, 0);
}

int read(int file, void* ptr, size_t len) {
	sc(12, file, ptr, len, 0, 0);
}

ssize_t readlink(const char *__restrict path, char* buf, size_t bufsize) {
	sc(13, path, buf, bufsize, 0, 0);
}

void* sbrk(ptrdiff_t incr) {
	extern char   end; /* Set by linker.  */
	static char * heap_end; 
	char *        prev_heap_end; 

	if (heap_end == 0)
		heap_end = &end; 

	prev_heap_end = heap_end; 
	heap_end += incr; 

	return (void *) prev_heap_end;
}

int stat(const char* file, struct stat* st) {
	sc(14, file, st, 0, 0, 0);
}

int symlink(const char* path1, const char* path2) {
	sc(15, path1, path2, 0, 0, 0);
}

clock_t times(struct tms* buf) {
	sc(16, buf, 0, 0, 0, 0);
}

int unlink(const char* name) {
	sc(17, name, 0, 0, 0, 0);
}

int wait(int* status) {
	sc(18, status, 0, 0, 0, 0);
}

int write(int file, const void* ptr, size_t len) {
	sc(19, file, ptr, len, 0, 0);
}

struct dirent* readdir(DIR* dir) {
	dir->position += 1;
	sc(20, dir->fd, dir->position - 1, 0, 0, 0);
}

int ioctl(int fd, int request, void* buf) {
	sc(21, fd, request, buf, 0, 0);
}

int mount(const char* dev, const char* dir, const char* fstype, unsigned int options, const void* data) {
	sc(22, dev, dir, fstype, options, data);
}

int umount2(const char* dir, int flags) {
	sc(23, dir, flags, 0, 0, 0);
}

int umount(const char* dir) {
	return umount2(dir, 0);
}

int tell(int fd) {
	sc(24, fd, 0, 0, 0, 0);
}

char** __get_argv() {
	sc(25, 0, 0, 0, 0, 0);
}

char** __get_env() {
	sc(26, 0, 0, 0, 0, 0);
}

int pipe(int fd[2]) {
	sc(27, fd, 0, 0, 0, 0);
}

int chdir(const char* path) {
	sc(28, path, 0, 0, 0, 0);
}

int __install_signal_handler(void* handler) {
	sc(29, handler, 0, 0, 0, 0);
}


void* _malloc_r(struct _reent* reent, size_t size) {
	sc(30, size, 0, 0, 0, 0);
}

void _free_r(struct _reent* reent, void* ptr) {
	sc_noret(31, ptr, 0, 0, 0, 0);
}

void* _calloc_r(struct _reent* reent, size_t nmemb, size_t size) {
	return _malloc_r(reent, nmemb * size);
}


void* _realloc_r(struct _reent* reent, void* ptr, size_t size) {
	if(size == 0) {
		_free_r(reent, ptr);
		return NULL;
	}
	
	void* ret = _malloc_r(reent, size);
	memcpy(ret, ptr, size);
	
	_free_r(reent, ptr);
	return ret;
}

int aplus_thread_create(uint32_t entry, void* param, int priority) {
	sc(32, entry, param, priority, 0, 0);
}

void aplus_thread_idle() {
	sc_noret(33, 0, 0, 0, 0, 0);
}

void aplus_thread_wakeup() {
	sc_noret(34, 0, 0, 0, 0, 0);
}

void aplus_thread_zombie() {
	sc_noret(35, 0, 0, 0, 0, 0);
}

void* aplus_device_create(char* path, int mode) {
	sc(36, path, mode, 0, 0, 0);
}



DIR* opendir(const char* name) {
	int fd = open(name, O_RDONLY, S_IFDIR);
	if(fd < 0) {
		errno = -ENOENT;
		return 0;
	}
	
	DIR* d = malloc(sizeof(DIR));
	d->fd = fd;
	d->position = 0;
	
	return d;
}

int closedir(DIR* dir) {
	if(!dir) {
		errno = -EINVAL;
		return -1;
	}

	close(dir->fd);
	free(dir);
	
	return 0;
}

void rewinddir(DIR* dir) {
	if(!dir) {
		errno = -EINVAL;
		return;
	}

	dir->position = 0;
	return 0;
}

void seekdir(DIR* dir, off_t offset) {
	if(!dir) {
		errno = -EINVAL;
		return;
	}

	dir->position = offset;
}

off_t telldir(DIR* dir) {
	if(!dir) {
		errno = -EINVAL;
		return -1;
	}

	return dir->position;
}

int scandir(const char *pathname, struct dirent ***namelist, int (*select)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **)) {
	/* WTF */
	return -1;
}





void __signal_handler__(int sig) {
	raise(sig);
}


#if 0 /* Kernel Land */

void _start() {
	char** argv = __get_argv();
	int argc = 0;
	
	while(argv[argc])
		argc++;
		
	__install_signal_handler(__signal_handler__);
	_init_signal();
	
	extern int main(int argc, char** argv);
	_exit(main(argc, argv));
}

#endif

