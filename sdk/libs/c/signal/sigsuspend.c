#include <signal.h>
#include <errno.h>

int sigsuspend(const sigset_t *mask) {
    errno = ENOSYS;
    return -1;
}