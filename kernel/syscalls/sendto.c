#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

ssize_t sys_sendto(int fd, const void* buffer, size_t len, int flags, const struct sockaddr* addr, socklen_t addrlen) {
#if CONFIG_NETWORK
    return lwip_sendto(fd - TASK_FD_COUNT, buffer, len, flags, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;
}

SYSCALL(83, _sendto,
ssize_t sys__sendto(uintptr_t* p) {
    return sys_sendto(
        (int) p[0],
        (const void*) p[1],
        (size_t) p[2],
        (int) p[3],
        (const struct sockaddr*) p[4],
        (socklen_t) p[5]
    );
});

EXPORT(sys_sendto);