#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <libc.h>

SYSCALL(70, socket,
int sys_socket(int domain, int type, int protocol) {
#if CONFIG_NETWORK
    int fd = lwip_socket(domain, type, protocol);
    if(fd < 0)
        return -1;
    
    return fd + TASK_FD_COUNT;
#endif

    errno = ENOSYS;
    return -1;
});