#include "syscalls.h"

int sys_open(const char* name, int flags, int mode) {
	errno = ENOSYS;	
	return -1;
}
