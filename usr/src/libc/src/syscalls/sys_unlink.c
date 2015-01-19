#include "syscalls.h"

int sys_unlink(const char* name) {
	errno = ENOSYS;
	return -1;
}
