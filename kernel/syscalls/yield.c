#include <xdev.h>
#include <xdev/task.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>


SYSCALL(23, yield,
void sys_yield(void) {
	arch_task_yield();
});
