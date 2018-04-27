#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int pcsrv_listen(int fd) {
    if(fd < 0)
        return -1;


    do {
        char ln;
        if(read(fd, &ln, sizeof(ln)) != sizeof(ln)) {
            fprintf(stderr, "pcsrv: I/O error: %s:%d\n", __FILE__, __LINE__);
            continue;
        }

        char nm[ln];
        if(read(fd, nm, ln) != ln) {
            fprintf(stderr, "pcsrv: I/O error: %s:%d\n", __FILE__, __LINE__);
            continue;
        }

        //pcsrv_accept(nm);
    } while(1);
}