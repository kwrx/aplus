#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <sched.h>
#include <errno.h>

#include "pthread_internal.h"

int	pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
    errno = ENOSYS;
    return -1;
}