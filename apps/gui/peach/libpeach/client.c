#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <peach/peach.h>


int peach_subscribe(int pipe[2], int index) {
    int fd = -1;
    int sd = -1;

    char svpath[BUFSIZ];
    char tmpipe[BUFSIZ];

    
    sprintf(svpath, "/tmp/peach.%d", index);

    if((sd = open(svpath, O_RDWR)) < 0) {
        fprintf(stderr, "peach: server not running, could not connect\n");
        abort();
    }
    

    do {
        tmpnam(tmpipe);

        if(mkfifo(tmpipe, 0666) == 0)
            fd = open(tmpipe, O_RDWR);
    } while (fd < 0 && errno == EEXIST);

    if(fd < 0) {
        fprintf(stderr, "peach: could not create client pipe\n");
        abort();
    }

    peach_msg_t msg;
    msg.magic = PEACH_MSG_MAGIC;
    msg.type = PEACH_MSG_SUBSCRIBE;
    msg.size = strlen(tmpipe);

    write(sd, &msg, sizeof(msg));
    write(sd, tmpipe, strlen(tmpipe));

#if DEBUG
    fprintf(stderr, "peach: established connection on %s\n", tmpipe);
#endif

    pipe[0] = fd;
    pipe[1] = sd;

    return 0;
}
