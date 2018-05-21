#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <libc.h>

SYSCALL(91, init_module,
int sys_init_module(void* image, unsigned long len, const char* param_values) {
    if(unlikely(!image || !len)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(current_task->uid != TASK_ROOT_UID)) {
        errno = EPERM;
        return -1;
    }

    char* name;
    if(module_check(image, (size_t) len, &name) != 0)
        return -1;

    if(module_load(name) != 0)
        return -1;

    if(module_run(name) != 0)
        return -1;

    return 0;
});