#include <time.h>
#include <sys/time.h>
#include <errno.h>

int clock_gettime(clockid_t clkid, struct timespec* tp) {
    switch(clkid) {
        case CLOCK_REALTIME:
#ifdef CLOCK_MONOTONIC
        case CLOCK_MONOTONIC:
#endif
            {
                struct timeval tv;
                if(gettimeofday(&tv, NULL) != 0)
                    return -1;

                tp->tv_sec = tv.tv_sec;
                tp->tv_nsec = tv.tv_usec * 1000;

                return 0;
            }
            break;
#ifdef CLOCK_PROCESS_CPUTIME_ID
        case CLOCK_PROCESS_CPUTIME_ID:
#endif
#ifdef CLOCK_THREAD_CPUTIME_ID
        case CLOCK_THREAD_CPUTIME_ID:
#endif
        default:
            errno = ENOSYS;
            return -1;
    }

    errno = EINVAL;
    return -1;
}