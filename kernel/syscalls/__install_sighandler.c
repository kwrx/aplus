#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(102, __install_sighandler,
void sys___install_sighandler(int (*handler) (int)) {
	KASSERT(current_task);

	current_task->sig_handler = handler;
});
