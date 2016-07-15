#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(5, getpid,
pid_t sys_getpid(void) {
	if(unlikely(!current_task))
		return -1;

	return current_task->pid;
});
