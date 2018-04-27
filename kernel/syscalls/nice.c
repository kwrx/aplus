#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(60, nice,
int sys_nice(int incr) {
    KASSERT(current_task);

    if((incr < 0) && (current_task->uid != TASK_ROOT_UID)) {
        errno = EPERM;
        return -1;
    }

    if(incr > TASK_PRIO_MIN || incr < TASK_PRIO_MAX) {
        errno = EINVAL;
        return -1;
    }

    current_task->priority = incr;
    return 0; 
});