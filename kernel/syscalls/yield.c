#include <aplus.h>
#include <aplus/task.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>


SYSCALL(23, _yield,
void sys__yield(void) {
	schedule_yield();
});


void sys_yield(void) {
	syscall_ack();
	arch_task_yield();
}

EXPORT(sys_yield);