
#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/fs.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

int sys_link(char* filename, char* link) {
	/* TODO */
	kprintf("sys_link: TODO");

	errno = ENOSYS;
	return -1;
}

SYSCALL(sys_link, 8);
