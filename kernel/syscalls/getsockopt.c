#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(76, getsockopt,
int sys_getsockopt(int fd, int lvl, int opt, void *restrict val, socklen_t *restrict len) {
#if CONFIG_NETWORK
    return lwip_getsockopt(fd - TASK_FD_COUNT, lvl, opt, val, len);
#endif

    errno = ENOSYS;
    return -1;
});