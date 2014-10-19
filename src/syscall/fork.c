#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int sys_fork() {
	task_t* child = (task_t*) task_fork();
	
	if(child)
		return child->pid;
	else
		return 0;
}


SYSCALL(sys_fork, 3);
