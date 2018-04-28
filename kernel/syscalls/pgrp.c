#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(47, setpgid,
int sys_setpgid(pid_t pid, gid_t pgid) {
    if(pid == 0)
        current_task->pgid = pgid == 0 
                                ? current_task->pid 
                                : pgid
                                ;
    else {
        volatile task_t* tmp;
        for(tmp = task_queue; tmp; tmp = tmp->next) {
            if(tmp->pid == pid) {
                if(tmp->sid != current_task->sid) {
                    errno = EPERM;
                    return -1;
                }
                
                tmp->pgid = pgid == 0 
                                ? tmp->pid 
                                : pgid
                                ;
                return 0;   
            }
        }
    
        errno = ESRCH;
        return -1;
    }

    return 0;
});

SYSCALL(48, getpgid,
gid_t sys_getpgid(pid_t pid) {
    if(pid == 0)
        return current_task->pgid;
    else {
        volatile task_t* tmp;
        for(tmp = task_queue; tmp; tmp = tmp->next) {
            if(tmp->pid == pid)
                return tmp->pgid;
        }
    }

    errno = ESRCH;
    return -1;
});
