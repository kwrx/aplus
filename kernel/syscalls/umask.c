#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>


SYSCALL(41, umask,
mode_t sys_umask(mode_t cmask) {
	KASSERT(current_task);

	register mode_t omask = current_task->umask;
	current_task->umask = cmask;
	return omask;
});
