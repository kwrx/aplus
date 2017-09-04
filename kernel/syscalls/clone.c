#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(22, clone,
int sys_clone(int (*fn)(void *), void *child_stack, int flags, void *arg) {
    volatile task_t* child = arch_task_clone(fn, child_stack, flags, arg);
    if(unlikely(!child)) {
        errno = EINVAL;
        return -1;
    }

    return child->pid;
});
