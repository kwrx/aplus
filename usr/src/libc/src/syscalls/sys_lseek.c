#include "syscalls.h"

int sys_lseek(int fd, int offset, int dir) {
	errno = ENOSYS;
	return -1;
}
