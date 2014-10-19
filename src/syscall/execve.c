#include <aplus.h>
#include <aplus/syscall.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


int sys_execve(char* filename, char** argv, char** environ) {
	errno = ENOSYS;
	return -1;
}


SYSCALL(sys_execve, 2);
