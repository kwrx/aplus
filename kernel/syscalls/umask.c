#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>


SYSCALL(41, umask,
mode_t sys_umask(mode_t cmask) {
    KASSERT(current_task);

    register mode_t omask = current_task->umask;
    current_task->umask = cmask;
    return omask;
});
