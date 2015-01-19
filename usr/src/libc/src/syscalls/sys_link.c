#include "syscalls.h"

int sys_link(const char* oldname, const char* newname) {
	errno = ENOSYS;
	return -1;
}
