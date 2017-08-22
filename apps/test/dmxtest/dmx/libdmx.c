#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <aplus/base.h>
#include <aplus/kmem.h>
#include <aplus/input.h>
#include <aplus/msg.h>

#include "lib/list.h"
#include "libdmx.h"

list(dmx_context_t*, contexts);
char* dmx_errno = NULL;


static void dmx_sighandler(int sig) {


    signal(sig, dmx_sighandler);
}

static void dmx_atexit() {
    list_each(contexts, ctx)
        dmx_context_close(ctx);
    
    list_clear(contexts);
}


int dmx_open(const char* path) {
    int fd = open(path, O_RDWR);
    if(fd < 0) {
        LIBDMX_ERROR("Could not connect to Server", 0);
        return -1;
    }

    signal(SIGMSG, dmx_sighandler);
    atexit(dmx_atexit);

    return fd;
}


int dmx_close(int fd) {
    dmx_atexit();

    signal(SIGMSG, SIG_DFL);
    close(fd);

    return 0;
}

__attribute__((noreturn))
void dmx_throw() {
    if(dmx_errno)
        fprintf(stderr, "dmx: %s: %s\n", dmx_errno, strerror(errno));
    else
        fprintf(stderr, "dmx: %s\n", strerror(errno));

    exit(errno);
}