#include <aplus.h>
#include <aplus/tty.h>
#include <aplus/syscall.h>

#include <errno.h>


int sys_openpty(int* master, int* slave, char* name, const struct termios* ios, const struct winsize* win) {
	if(unlikely(!master || !slave)) {
		errno = EINVAL;
		return -1;
	}

	return tty_open(master, slave, name, ios, win);
}

SYSCALL(sys_openpty, 27);
