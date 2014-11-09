#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


/**
 *	\brief fork() creates a new process by duplicating the calling process.\n
		The new process, referred to as the child, is an exact duplicate of the
		calling process, referred to as the parent.
 *	\return On success, the PID of the child process is returned in the parent,
       	and 0 is returned in the child.\n
		On failure, -1 is returned in the parent, no child process is created, 
		and errno is set appropriately.
 */
int sys_fork() {
	task_t* child = (task_t*) task_fork();
	
	if(child)
		return child->pid;
	else
		return 0;
}


SYSCALL(sys_fork, 3);
