#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <libc.h>

SYSCALL(84, gethostbyname,
struct hostent* sys_gethostbyname(const char* name) {
#if CONFIG_NETWORK
    return lwip_gethostbyname(name);
#endif

    errno = ENOSYS;
    return NULL;
});