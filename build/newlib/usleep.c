#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>
#include <errno.h>

int usleep(useconds_t usec) {
    if(usec >= 1000000) {
        errno = EINVAL;
        return -1;
    }
    
    struct timespec tq, tr;
    tq.tv_sec = 0;
    tq.tv_nsec = usec * 1000;
    
    if(nanosleep(&tq, &tr) == 0)
        return 0;
        
    return -1;
}
