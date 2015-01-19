#include "syscalls.h"

int sys_read(int fd, void* buf, size_t size) {
	errno = ENOSYS;
	return -1;
}
