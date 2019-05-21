#include <signal.h>

typedef void (*__sighandler_t) (int);

__sighandler_t signal (int sig, __sighandler_t handler) {
    struct sigaction ss, so;
    if(rt_sigaction(sig, NULL, &so, sizeof(so.sa_mask)) != 0)
        return NULL;

    ss.sa_handler = handler;
    ss.sa_mask = so.sa_mask;
    ss.sa_flags = SA_RESETHAND;
    ss.sa_restorer = NULL;

    if(rt_sigaction(sig, &ss, NULL, sizeof(ss.sa_mask)) != 0)
        return NULL;

    return so.sa_handler;
}