#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(78, connect,
int sys_connect(int fd, const struct sockaddr* addr, socklen_t addrlen) {
#if CONFIG_NETWORK
    return lwip_connect(fd - TASK_FD_COUNT, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;
});