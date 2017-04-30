#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(45, getsid,
int sys_getsid(void) {
    KASSERT(current_task);

	return current_task->sid;
});