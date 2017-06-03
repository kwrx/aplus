#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

SYSCALL(52, setgid,
int sys_setgid(gid_t gid) {
	KASSERT(current_task);

    current_task->gid = gid;
    return 0;
});
