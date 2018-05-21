#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <libc.h>

SYSCALL(92, delete_module,
int sys_delete_module(char* name, int flags) {
    if(unlikely(!name)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(!(flags & O_NONBLOCK))) {
        errno = ENOSYS;
        return -1;
    }

    if(unlikely(current_task->uid != TASK_ROOT_UID)) {
        errno = EPERM;
        return -1;
    }


    return module_exit(name);
});