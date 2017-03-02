#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

ssize_t sys_recvfrom(int fd, void *restrict buffer, size_t len, int flags, struct sockaddr *restrict addr, socklen_t *restrict addrlen) {
#if CONFIG_NETWORK
    return lwip_recvfrom(fd - TASK_FD_COUNT, buffer, len, flags, addr, addrlen);
#endif

    errno = ENOSYS;
    return -1;
}

SYSCALL(82, _recvfrom,
ssize_t sys__recvfrom(uintptr_t* p) {
    return sys_recvfrom(
        (int) p[0],
        (void*) p[1],
        (size_t) p[2],
        (int) p[3],
        (struct sockaddr*) p[4],
        (socklen_t*) p[5]
    );
});

EXPORT(sys_recvfrom);