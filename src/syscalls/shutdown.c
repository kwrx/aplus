#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(72, shutdown,
int sys_shutdown(int fd, int how) {
#if CONFIG_NETWORK
    return lwip_shutdown(fd - TASK_FD_COUNT, how);
#endif

    errno = ENOSYS;
    return -1;
});