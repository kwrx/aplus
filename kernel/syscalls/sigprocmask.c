#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(49, sigprocmask,
int sys_sigprocmask(int how, sigset_t* set, sigset_t* oldset) {
    if(oldset)
        *oldset = current_task->signal.s_mask;

    switch(how) {
        case SIG_BLOCK:
            current_task->signal.s_mask |= *set;
            break;
        case SIG_UNBLOCK:
            current_task->signal.s_mask &= ~(*set);
            break;
        case SIG_SETMASK:
            current_task->signal.s_mask = *set;
            break;
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
});
