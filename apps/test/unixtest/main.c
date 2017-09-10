#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <errno.h>


#define __test(x, y, z) {                                                                       \
    y r;                                                                                        \
    fprintf(stderr, "\e[37m" #x ": \e[39m");                                                    \
    if((r = (x)) == z)                                                                          \
        fprintf(stderr, "failed, returned %p, errno: %d (%s)\n", r, errno, strerror(errno));    \
    else                                                                                        \
        fprintf(stderr, "success: returned %p\n", r);                                           \
}

int main(int argc, char** argv) {
    pid_t pid;

    __test(sleep(1), unsigned int, -1);
    __test(sleep(1), unsigned int, -1);
    __test(sleep(1), unsigned int, -1);
    __test(sleep(1), unsigned int, -1);
    __test(sleep(1), unsigned int, -1);
    __test(usleep(100), int, -1);
    //__test(posix_spawn(&pid, "/usr/bin/echo", NULL, NULL, argv, environ), int, -1);
    __test(getlogin(), char*, NULL);
    //__test(getpass("root"), char*, NULL);
    __test(ttyname(0), char*, NULL);
    __test(getpwnam("root"), struct passwd*, NULL);

    return 0;
}