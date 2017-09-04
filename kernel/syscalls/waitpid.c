#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <libc.h>


#define __wait_for(cond)                                            \
    {                                                               \
        volatile task_t* v;                                         \
        for(v = task_queue; v; v = v->next) {                       \
            if(v->parent != current_task)                           \
                continue;                                           \
                                                                    \
            if((cond))                                              \
                list_push(current_task->waiters, v);                \
        }                                                           \
    }


SYSCALL(31, waitpid,
pid_t sys_waitpid(pid_t pid, int* status, int options) {
    if(pid < -1)
        __wait_for(v->gid == -pid)
    else if(pid == -1)
        __wait_for(1)
    else if(pid == 0)
        __wait_for(v->gid == current_task->gid)
    else if(pid > 0)
        __wait_for(v->pid == pid)


    if(list_length(current_task->waiters) == 0) {
        errno = ECHILD;
        return -1;
    }


    syscall_ack();

    current_task->status = TASK_STATUS_SLEEP;
    while(current_task->status == TASK_STATUS_SLEEP)
        sys_yield();


    pid_t p = -1;
    list_each(current_task->waiters, w) {
        if(w->status != TASK_STATUS_KILLED)
            continue;
            
        if(status)
            *status = (w->exit.status << 16) | (w->exit.value & 0xFFFF);
        
        p = w->pid;
        break;
    }

    list_clear(current_task->waiters);
    return p;
});
