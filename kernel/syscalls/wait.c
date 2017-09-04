#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <libc.h>


SYSCALL(16, wait,
pid_t sys_wait(int* status) {
    return sys_waitpid(-1, status, 0);
});
