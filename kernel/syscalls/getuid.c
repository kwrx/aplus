#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(43, getuid,
int sys_getuid(void) {
    KASSERT(current_task);

	return current_task->uid;
});