#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/debug.h>
#include <aplus/intr.h>
#include <libc.h>

SYSCALL(0, exit,
void sys_exit(int status) {
    KASSERT(current_task != kernel_task);
    
    if(status & (1 << 31))
        current_task->exit.value = status & 0x7FFF;
    else
        current_task->exit.value = (status & 0377) << 8;    


#if DEBUG
    kprintf(INFO "exit: task %d (%s) %s with %04X (U: %0.3fs, C: %0.3fs, VM: %d)\n", 
        current_task->pid, 
        current_task->name,
        WIFSTOPPED(current_task->exit.value) ? "stopped" : "exited",
        current_task->exit.value & 0xFFFF,
        (double) current_task->clock.tms_utime / CLOCKS_PER_SEC,
        (double) current_task->clock.tms_cutime / CLOCKS_PER_SEC,
        current_task->image->refcount
    );   
#endif


    current_task->status = (WIFSTOPPED(current_task->exit.value))
        ? TASK_STATUS_STOP
        : TASK_STATUS_ZOMBIE;


    if(current_task->parent)
        sched_signal(current_task->parent, SIGCHLD);



    if(current_task->status == TASK_STATUS_STOP) {
        while(current_task->status == TASK_STATUS_STOP)
            sys_yield();

        return;
    }


    INTR_OFF;

    volatile task_t* tmp;
    for(tmp = task_queue; tmp; tmp = tmp->next)
        if(tmp->parent == current_task)
            tmp->parent = kernel_task;

    
    int i;
    for(i = 0; i < TASK_FD_COUNT; i++)
        sys_close(i);


    if((--current_task->image->refcount) == 0)
        task_release(current_task);

    syscall_ack();

    INTR_ON;
    for(;;) 
        sys_yield();
});
