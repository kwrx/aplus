#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(51, setuid,
int sys_setuid(uid_t uid) {
    KASSERT(current_task);


    current_task->uid = uid;
    return 0;
});
