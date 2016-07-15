#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>



SYSCALL(35, munmap,
void* sys_munmap(void* addr, size_t len) {
	errno = ENOSYS;	
	return NULL;
});
