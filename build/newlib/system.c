#include <stdlib.h>
#include <unistd.h>


int system(const char* s) {
    char* argv[4];
    argv[0] = "/usr/bin/sh";
    argv[1] = "-c";
    argv[2] = s;
    argv[3] = NULL;

    int e = fork();
    if(e == 0) {
        execve(argv[0], argv, environ);
        exit(100);
    } else if(e == -1)
        return -1;

    int st;
    e = wait(&st);
    if(e == -1)
        return -1;

    st = (st >> 8) & 0xFF;
    return st;
}