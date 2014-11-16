
#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>


extern task_t* current_task;

pid_t sys_getpid() {
	return schedule_getpid();
}

pid_t sys_getppid() {
	if(!current_task)
		return -1;

	if(current_task->parent)
		return current_task->parent->pid;

	errno = ESRCH;
	return -1;
}


SYSCALL(sys_getpid, 5);
SYSCALL(sys_getppid, 32);
