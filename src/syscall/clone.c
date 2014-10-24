#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <errno.h>

extern task_t* current_task;

int sys_clone(int (*fn)(void*), void* child_stack, int flags, void* arg) {
	if(!current_task)
		return -1;
	
	if(fn == NULL) {
		errno = EINVAL;
		return -1;
	}


	task_t* child = (task_t*) task_clone(fn, arg, child_stack);
	if(!child) {
		errno = EFAULT;
		return -1;
	}

	return child->pid;
}

SYSCALL(sys_clone, 22);
