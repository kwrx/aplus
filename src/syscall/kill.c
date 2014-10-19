#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


int sys_kill(pid_t pid, int signal) {
	task_t* task = (task_t*) schedule_getbypid(pid);
	if(!task) {
		errno = ESRCH;
		return -1;
	}
	
	task->signal_sig = signal;
	return 0;
}

SYSCALL(sys_kill, 7);
