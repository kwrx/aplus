#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(102, __install_sighandler,
void sys___install_sighandler(int (*handler) (int)) {
	KASSERT(current_task);

	current_task->sig_handler = handler;
});
