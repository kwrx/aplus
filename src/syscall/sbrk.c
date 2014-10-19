#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/task.h>


void* sys_sbrk(ptrdiff_t increment) {
	return (void*) schedule_sbrk(increment);
}

SYSCALL(sys_sbrk, 12);
