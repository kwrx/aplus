#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
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