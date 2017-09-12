#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(57, pause,
int sys_pause(void) {
    KASSERT(current_task);

    
    syscall_ack();
    
    current_task->status = TASK_STATUS_SLEEP;
    while(current_task->status == TASK_STATUS_SLEEP)
        sys_yield();

    errno = EINTR;
    return -1;
});