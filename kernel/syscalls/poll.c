#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/network.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(85, poll,
int sys_poll(struct pollfd* fds, nfds_t nfds, int timeout) {
    if(unlikely(!fds)) {
        errno = EINVAL;
        return -1;
    }


	int i;
    for(i = 0; i < nfds; i++) {
        if(fds[i].fd < 0) {
            fds[i].revents = 0;
            continue;
        }
    }

    return nfds;
});
