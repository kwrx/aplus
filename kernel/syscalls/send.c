#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(81, send,
int sys_send(int fd, const void* buffer, size_t len, int flags) {
#if CONFIG_NETWORK
    return lwip_send(fd - TASK_FD_COUNT, buffer, len, flags);
#endif

    errno = ENOSYS;
    return -1;
});