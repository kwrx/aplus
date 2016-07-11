#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(73, accept,
int sys_accept(int fd, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
#if CONFIG_NETWORK
    return lwip_accept(fd - TASK_FD_COUNT, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;   
});