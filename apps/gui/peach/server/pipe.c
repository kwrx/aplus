#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


int pcsrv_init_pipe(int display, int number) {
    char tmp[BUFSIZ];
    sprintf(tmp, "/tmp/peach:%d.%d", display, number);

    if(mkfifo(tmp, 0666) != 0) {
        fprintf(stderr, "pcsrv: server already running, could not create: %s\n", tmp);
        abort();
    }

    int fd = open(tmp, O_RDWR);
    if(fd < 0) {
        fprintf(stderr, "pcsrv: could not open server pipe: %s\n", tmp);
        abort();
    }

    return fd;
}