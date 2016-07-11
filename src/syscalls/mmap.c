#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <libc.h>

void* sys_mmap(void* addr, size_t len, int prot, int flags, int fildes, off_t off) {
	(void) fildes;
	(void) off;


	if(unlikely(!len)) {
		errno = EINVAL;
		return NULL;
	}

	len &= ~(PAGE_SIZE - 1);
	len += PAGE_SIZE;


	uintptr_t rd = 0;
	if(flags & MAP_FIXED)
		rd = (uintptr_t) addr;
	else {
		if(flags & MAP_PRIVATE)
			rd = (uintptr_t) sys_sbrk(len);
		else
			rd = (uintptr_t) kvalloc(len, GFP_ATOMIC);
	}

	if(unlikely(!rd)) {
		errno = ENOMEM;
		return NULL;
	}

	
	rd &= ~(PAGE_SIZE - 1);


	uintptr_t i;
	for(i = 0; i < len; i += PAGE_SIZE)
		map_page(rd + i, pmm_alloc_frame() << 12, 1);


	if(flags & MAP_ANON)
		memset((void*) rd, 0, len);

	return (void*) rd;
}

SYSCALL(34, _mmap,
	void* sys__mmap(uintptr_t* p) {
		return sys_mmap(
			(void*) p[0],
			(size_t) p[1],
			(int) p[2],
			(int) p[3],
			(int) p[4],
			(off_t) p[5]
		);
	}
);

EXPORT(sys_mmap);