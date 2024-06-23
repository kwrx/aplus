#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <wc/wc.h>
#include <wc/wc_event.h>


static int evpipe[2] = {-1, -1};

int wc_event_initialize(void) {

    assert(evpipe[0] == -1);
    assert(evpipe[1] == -1);

    if (pipe(evpipe) < 0) {
        return -1;
    }

    LOG("event subsystem initialized\n");

    return 0;
}


int wc_event_wait(wc_event_t *type) {

    assert(evpipe[0] != -1);
    assert(evpipe[1] != -1);


    wc_event_t buf = {0};

    if (read(evpipe[0], &buf, sizeof(wc_event_t)) < 0) {
        return -1;
    }

    if (type) {
        *type = buf;
    }

    return 0;
}


int wc_event_post(wc_event_t type) {

    assert(evpipe[0] != -1);
    assert(evpipe[1] != -1);

    if (write(evpipe[1], &type, sizeof(wc_event_t)) < 0) {
        return -1;
    }

    return 0;
}
