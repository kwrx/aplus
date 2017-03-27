#include "appx.h"


void appx_lck_acquire() {
    int fd = open(APPX_LOCK, O_CREAT | O_EXCL, 0644 | S_IFREG);
    if(fd < 0) {
        fprintf(stderr, "appx: another instance of APPX is running, wait or remove lock manually:\n\trm -f %s\n", APPX_LOCK);
        abort();
    }

    close(fd);
}


void appx_lck_release() {
    if(unlink(APPX_LOCK) == 0)
        return;

    perror("appx: error on releasing lock file");
    fprintf(stderr, "Please try to remove manually:\n\trm -f %s\n", APPX_LOCK);
}