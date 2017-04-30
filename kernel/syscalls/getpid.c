#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(5, getpid,
pid_t sys_getpid(void) {
	if(unlikely(!current_task))
		return 0;

	return current_task->pid;
});
