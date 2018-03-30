#define _POSIX_MONOTONIC_CLOCK
#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>



SYSCALL(56, nanosleep,
int sys_nanosleep(struct timespec* req, struct timespec* rem) {
    if(!req) {
        errno = EINVAL;
        return -1;
    }
    
    if(req->tv_nsec < 0 || req->tv_nsec > 999999999) {
        errno = EINVAL;
        return -1;
    }
    
    
    struct timespec t0;
    if(sys_clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        errno = EINVAL;
        return -1;
    }
    
    
    
    current_task->sleep.tv_sec = req->tv_sec + t0.tv_sec;
    current_task->sleep.tv_nsec = (req->tv_nsec + t0.tv_nsec) % 1000000000;

    if((req->tv_nsec + t0.tv_nsec) > 1000000000)
        current_task->sleep.tv_sec++;

    
    syscall_ack();
    sys_pause();


    uint64_t tv_sec = current_task->sleep.tv_sec;
    uint64_t tv_nsec = current_task->sleep.tv_nsec;


    current_task->sleep.tv_sec =
    current_task->sleep.tv_nsec = 0;


    if(sys_clock_gettime(CLOCK_MONOTONIC, &t0) != 0) {
        errno = EINVAL;
        return -1;
    }

    if((tv_sec * 1000000000ULL + tv_nsec) > (t0.tv_sec * 1000000000ULL + t0.tv_nsec)) {
        if(rem) {
            rem->tv_sec = tv_sec - t0.tv_sec;
            rem->tv_nsec = tv_nsec - t0.tv_nsec;
        }

        errno = EINTR;
        return -1;
    }

    return 0;
});