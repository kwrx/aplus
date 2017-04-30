#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(46, alarm,
unsigned int sys_alarm(unsigned int seconds) {
    KASSERT(current_task);

    unsigned int e = 0;
    if(current_task->alarm)
        e = current_task->alarm - timer_gettime();

    if(seconds)
        current_task->alarm = timer_gettime() + seconds;
    else
        current_task->alarm = 0;
        
    return e;
});