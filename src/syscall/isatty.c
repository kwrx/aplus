#include <aplus.h>
#include <aplus/syscall.h>


int sys_isatty(int fd) {
	return (fd < 3);
}

SYSCALL(sys_isatty, 6);
