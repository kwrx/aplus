#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <libc.h>


SYSCALL(3, fork,
int sys_fork(void) {
	volatile task_t* child = arch_task_fork();
	
	if(child)
		return child->pid;

	return 0;
});
