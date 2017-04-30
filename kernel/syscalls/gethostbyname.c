#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
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