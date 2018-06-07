#include <unistd.h>

int setpgrp(void) {
    return setpgid(0, 0);
}

int getpgrp(void) {
    return getpgid(0);
}