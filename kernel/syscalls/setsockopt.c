#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(77, setsockopt,
int sys_setsockopt(int fd, int lvl, int opt, const void* val, socklen_t len) {
#if CONFIG_NETWORK
    return lwip_setsockopt(fd - TASK_FD_COUNT, lvl, opt, val, len);
#endif

    errno = ENOSYS;
    return -1;
});