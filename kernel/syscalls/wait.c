#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <libc.h>


SYSCALL(16, wait,
pid_t sys_wait(int* status) {
	return sys_waitpid(-1, status, 0);
});
