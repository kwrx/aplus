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

    if(timeout > 0)
        timeout += timer_getms();

	int i;
    for(;;) {
        for(i = 0; i < nfds; i++) {
            if(fds[i].fd < 0) {
                fds[i].revents = 0;
                continue;
            }

            inode_t* inode = current_task->fd[fds[i].fd].inode;
            if(!inode) {
                fds[i].revents |= POLLNVAL;
                continue;
            }

            if(!(S_ISFIFO(inode->mode)) || !inode->userdata) {
                fds[i].revents |= POLLERR;
                continue;
            }

            if(fds[i].events == 0)
                continue;

            fifo_t* fifo = (fifo_t*) inode->userdata;
            if(fifo_available(fifo) > 0)
                if(fds[i].events & POLLIN)
                    fds[i].revents |= POLLIN;

            if(fds[i].events & POLLOUT)
                fds[i].revents |= POLLOUT;
        }

        if(timeout == 0)
            break;

        if((timeout > 0) && (timer_getms() > timeout))
            break;

        sys_yield();
    }

    int n = 0;
    for(i = 0; i < nfds; i++)
        if(fds[i].revents != 0)
            n++;
            
    return n;
});
