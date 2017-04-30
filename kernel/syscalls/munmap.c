#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>



SYSCALL(35, munmap,
void* sys_munmap(void* addr, size_t len) {
	errno = ENOSYS;	
	return NULL;
});
