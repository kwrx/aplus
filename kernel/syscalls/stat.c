#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>


SYSCALL(13, stat,
int sys_stat(const char* name, struct stat* st) {
	int fd = sys_open(name, O_RDONLY, 0);
	if(fd < 0)
		return -1;

	register int r = fstat(fd, st);
	sys_close(fd);

	return r; 
});

SYSCALL(38, lstat,
int sys_lstat(const char* name, struct stat* st) {
	int fd = sys_open(name, O_RDONLY | O_NOFOLLOW, 0);
	if(fd < 0)
		return -1;

	register int r = fstat(fd, st);
	sys_close(fd);

	return r; 
});
