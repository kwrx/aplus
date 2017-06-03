#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(50, setsid,
int sys_setsid(void) {
	KASSERT(current_task);

    if(current_task->sid == current_task->pid) {
        errno = EPERM;
        return -1;
    }

    current_task->sid = current_task->pid;
    return 0;
});
