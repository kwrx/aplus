#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(59, select,
int sys_select(int nfds, fd_set* rfds, fd_set* wfds, fd_set* efds, struct timeval* timeout) {
#if CONFIG_NETWORK
    return lwip_select(nfds - TASK_FD_COUNT, rfds, wfds, efds, timeout);
#endif

    errno = ENOSYS;
    return -1;
});