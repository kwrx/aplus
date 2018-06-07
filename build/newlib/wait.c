#include <stdio.h>

int wait(int* status) {
    return waitpid(-1, status, 0);
}