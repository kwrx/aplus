#include <aplus.h>
#include <aplus/syscall.h>
#include <libc.h>


SYSCALL(6, isatty,
int sys_isatty(int fd) {
    return (fd < 3 && fd >= 0);
});
