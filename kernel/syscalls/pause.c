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
    ipc_wait(current_task->status == TASK_STATUS_SLEEP);

    errno = EINTR;
    return -1;
});