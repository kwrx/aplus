#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

SYSCALL(32, getppid,
pid_t sys_getppid(void) {
	if(current_task)
		if(current_task->parent)
			return current_task->parent->pid;

	return -1;
});
