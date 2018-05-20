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
        __wait_for(v->pgid == -pid)
    else if(pid == -1)
        __wait_for(1)
    else if(pid == 0)
        __wait_for(v->pgid == current_task->pgid)
    else if(pid > 0)
        __wait_for(v->pid == pid)


    if(list_length(current_task->waiters) == 0) {
        errno = ECHILD;
        return -1;
    }

    if(options & WNOHANG) {
        list_clear(current_task->waiters);
        return 0;
    }

    syscall_ack();
    sys_pause();


    pid_t p = -1;
    list_each(current_task->waiters, w) {
        if(w->status != TASK_STATUS_ZOMBIE && w->status != TASK_STATUS_STOP)
            continue;

        if(w->status == TASK_STATUS_ZOMBIE) {
            if(w == task_queue)
                task_queue = w->next;
            else {
                volatile task_t* tmp;
                for(tmp = task_queue; tmp; tmp = tmp->next) {
                    if(tmp->next == w)
                        tmp->next = w->next;
                }
            }
        }
            
        if(status)
            *status = (int) w->exit.value;
        
        p = w->pid;
        break;
    }

    list_clear(current_task->waiters);


    if(unlikely(p == -1))
        errno = EINTR;

    return p;
});
