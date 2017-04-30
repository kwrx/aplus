#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(74, getpeername,
int sys_getpeername(int fd, struct sockaddr* addr, socklen_t* addrlen) {
#if CONFIG_NETWORK
    return lwip_getpeername(fd - TASK_FD_COUNT, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;
});