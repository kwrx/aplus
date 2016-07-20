#include <xdev.h>
#include <xdev/task.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>


SYSCALL(23, _yield,
void sys__yield(void) {
	schedule_yield();
});


void sys_yield(void) {
	arch_task_yield();
}

EXPORT(sys_yield);