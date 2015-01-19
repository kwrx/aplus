#include "syscalls.h"

int sys_write(int fd, void* buf, size_t size) {
	errno = ENOSYS;
	return -1;
}
