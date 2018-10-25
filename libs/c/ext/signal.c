#include <signal.h>

typedef void (*__sighandler_t) (int);

__sighandler_t signal (int sig, __sighandler_t handler) {
    return NULL;
}