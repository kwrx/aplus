#include <xdev.h>
#include <xdev/syscall.h>
#include <libc.h>


SYSCALL(6, isatty,
int sys_isatty(int fd) {
	return (fd < 3 && fd >= 0);
});
