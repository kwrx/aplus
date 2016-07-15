#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>

SYSCALL(27, openpty,
int sys_openpty(int* amaster, int* aslave, char* name, const struct termios* temp, const struct winsize* winp) {
    errno = ENOSYS;
    return -1;
});