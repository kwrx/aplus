#include <stdint.h>
#include <sys/types.h>
#include "syscall.h"

void* __sbrk(intptr_t newbrk)
{
	return (void*) __syscall(SYS_sbrk, newbrk);
}
