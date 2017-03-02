#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <libc.h>

SYSCALL(32, getppid,
pid_t sys_getppid(void) {
	if(current_task)
		if(current_task->parent)
			return current_task->parent->pid;

	return -1;
});
