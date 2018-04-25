#include "pthread_internal.h"

int	pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void)) {
    errno = ENOSYS;
    return -1;
}