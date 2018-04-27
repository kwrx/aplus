#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


int peach_open(int display, int index) {
    int fd = -1;
    int sd = -1;

    char svpath[BUFSIZ];
    char tmpipe[BUFSIZ];

    
    sprintf(svpath, "/tmp/peach:%d.%d", display, index);

    if((sd = open(svpath, O_RDWR)) < 0) {
        fprintf(stderr, "peach: server not running, could not connect");
        abort();
    }
    


    do {
        tmpnam(tmpipe);

        if(mkfifo(tmpipe, 0666) == 0)
            fd = open(tmpipe, O_RDWR);
    } while (fd < 0 && errno == EEXIST);

    if(fd < 0) {
        fprintf(stderr, "peach: could not create client pipe");
        abort();
    }

    char ln = strlen(tmpipe);
    write(sd, &ln, sizeof(ln));
    write(sd, tmpipe, strlen(tmpipe));
    close(sd);

#if DEBUG
    fprintf(stderr, "peach: established connection on %s\n", tmpipe);
#endif

    return fd;
}
