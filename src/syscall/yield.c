#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


void sys_yield() {
	schedule_yield();
}

SYSCALL(sys_yield, 23);
