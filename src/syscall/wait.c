#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

extern task_t* current_task;

int sys_wait(int* status) {
	if(unlikely(current_task == NULL)) {
		errno = EFAULT;
		return -1;
	}

	return sys_waitpid(-1, status, 0);
}

SYSCALL(sys_wait, 16);
