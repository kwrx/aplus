#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <libc.h>


SYSCALL(7, kill,
int sys_kill(pid_t pid, int signal) {
    volatile task_t* tmp;
    int r = 0;

    #define _do(cond, ret) {                                        \
        for(tmp = task_queue; tmp; tmp = tmp->next) {               \
            if(current_task->uid != 0)                              \
                if(current_task->uid != tmp->uid)                   \
                    continue;                                       \
                                                                    \
            if(cond) {                                              \
                r++;                                                \
                if(signal == 0)                                     \
                    ret;                                            \
                                                                    \
                                                                    \
                if(tmp->sig_mask & (1 << signal)) {                 \
                    if(pid <= 0)                                    \
                        continue;                                   \
                                                                    \
                    errno = EPERM;                                  \
                    return -1;                                      \
                }                                                   \
                                                                    \
                tmp->sig_no = signal;                               \
                ret;                                                \
            }                                                       \
        }                                                           \
    }

    if(pid > 0)
        _do(tmp->pid == pid, return 0)
    else if(pid == 0)
        _do(tmp->gid == current_task->gid, continue)
    else if(pid == -1)
        _do(tmp != current_task, continue)
    else
        _do(tmp->gid == abs(pid), continue)
    


    if(r)
        return 0;
        
    errno = ESRCH;
    return -1;
});
