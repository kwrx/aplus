#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <libc.h>


SYSCALL(3, fork,
int sys_fork(void) {
    volatile task_t* child = task_fork();
    
    if(child)
        return child->pid;

    return 0;
});
