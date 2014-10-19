#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

extern task_t* current_task;

int sys_wait(int* status) {
	if(current_task == NULL) {
		errno = EFAULT;
		return -1;
	}

	task_t* child = (task_t*) schedule_child();
	if(child == NULL) {
		errno = ECHILD;
		return -1;
	}

	int exitcode = schedule_wait(child);

	if(status)
		*status = exitcode;

	return child->pid;
}

SYSCALL(sys_wait, 16);
