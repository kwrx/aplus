#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <unistd.h>
#include <errno.h>

unsigned int sleep(unsigned int seconds) {
    struct timespec tq, tr;
    tq.tv_sec = seconds;
    tq.tv_nsec = 0;
    
    if(nanosleep(&tq, &tr) == 0)
        return 0;
        
    if(errno == EINTR)
        return tr.tv_sec;
        
    return -1;
}
