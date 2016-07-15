#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(79, listen,
int sys_listen(int fd, int backlog) {
#if CONFIG_NETWORK
    return lwip_listen(fd - TASK_FD_COUNT, backlog);
#endif

    errno = ENOSYS;
    return -1;
});