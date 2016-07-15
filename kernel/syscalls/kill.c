#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <libc.h>


SYSCALL(7, kill,
int sys_kill(pid_t pid, int signal) {

	volatile task_t* tmp;
	for(tmp = task_queue; tmp; tmp = tmp->next) {
		if(tmp->pid == pid) {
			if(tmp->sig_mask & (1 << signal)) {
				errno = EPERM;
				return -1;
			}

			tmp->sig_no = signal;
			return 0;
		}
	}
	
	errno = ESRCH;
	return -1;
});
