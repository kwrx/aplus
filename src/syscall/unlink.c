#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

int sys_unlink(const char* pathname) {
	/* TODO */
	kprintf("sys_unlink: TODO");

	errno = ENOSYS;
	return -1;
}

SYSCALL(sys_unlink, 15);
