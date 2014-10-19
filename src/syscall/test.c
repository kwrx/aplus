#include <aplus.h>
#include <aplus/syscall.h>

int sys_test(int p0, int p1) {
	kprintf("sys_test called with (p0: %d; p1: %d)\n", p0, p1);

	return 255;
}

SYSCALL(sys_test, 1000);
