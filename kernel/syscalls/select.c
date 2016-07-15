#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/network.h>
#include <libc.h>

SYSCALL(36, select,
int sys_select(int nfds, fd_set* rfs, fd_set* wfs, fd_set* efs, struct timeval* timeout) {
#if CONFIG_NETWORK
	return lwip_select(nfds, rfs, wfs, efs, timeout);
#endif

    errno = ENOSYS;
    return -1;
});