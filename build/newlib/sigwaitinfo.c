#include <signal.h>

/* CRT0 */
extern int __last_signo;

int sigtimedwait(const sigset_t* set, siginfo_t* info, const struct timespec* timeout) {
    sigset_t old;
    sigprocmask(SIG_SETMASK, set, &old);

    if(timeout)
        nanosleep(timeout, NULL);
    else
        pause();

    sigprocmask(SIG_SETMASK, &old, NULL);
    return __last_signo;
}

int sigwaitinfo(const sigset_t* set, siginfo_t* info) {
    return sigtimedwait(set, info, NULL);
}