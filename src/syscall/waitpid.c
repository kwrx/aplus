#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/wait.h>
#include <sys/types.h>

extern task_t* current_task;

int sys_waitpid(pid_t pid, int* status, int options) {
	if(unlikely(current_task == NULL)) {
		errno = EFAULT;
		return -1;
	}


	task_t* child = NULL;

	if(pid == -1)
		child = (task_t*) schedule_child();
	else
		child = (task_t*) schedule_getbypid(pid);


	if(unlikely(child == NULL)) {
		errno = ECHILD;
		return -1;
	}

	if(options == WNOHANG)
		return child->pid;

	int exitcode = schedule_wait(child);

	if(status)
		*status = exitcode;

	return child->pid;
}

SYSCALL(sys_waitpid, 31);
