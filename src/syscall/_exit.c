#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


void sys_exit(int status) {
	schedule_exit(status);
}


SYSCALL(sys_exit, 0);