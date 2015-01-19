#include "syscalls.h"

int sys_close(int fd) {
	errno = ENOSYS;
	return -1;
}
