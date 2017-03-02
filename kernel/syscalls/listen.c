#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
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