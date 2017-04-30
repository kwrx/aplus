#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(27, openpty,
int sys_openpty(int* amaster, int* aslave, char* name, const struct termios* temp, const struct winsize* winp) {
    errno = ENOSYS;
    return -1;
});