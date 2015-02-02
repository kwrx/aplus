#ifdef __rpi__
#include <aplus.h>
#include <aplus/list.h>
#include <aplus/attribute.h>
#include <aplus/syscall.h>
#include <errno.h>

#define NSYSCALLS		1024

static void** syscall_handlers = NULL;


int syscall_init() {
	list_t* syslist = attribute("syscall");

	syscall_handlers = (void**) kmalloc(sizeof(void*) * NSYSCALLS);
	memset(syscall_handlers, 0, sizeof(void*) * NSYSCALLS);

	list_foreach(value, syslist) {
		syscall_t* sys = (syscall_t*) value;

		if(unlikely(syscall_handlers[sys->number]))
			kprintf("syscall: duplicate number of %d handler\n");

		syscall_handlers[sys->number] = sys->handler;
	}

	kprintf("syscall: loaded %d handlers\n", syslist->size);
	list_destroy(syslist);


#ifdef SYSCALL_DEBUG
	

	for(int i = 0, s = 0; i < NSYSCALLS; i++) {
		if(syscall_handlers[i] == NULL) {
			if(!s)
				continue;

			if(s > 1)
				kprintf("\t%d..%d\n", i - s, i - 1);
			else
				kprintf("\t%d\n", i - s);

			s = 0;
		} else
			s++;
	}
#endif


	return 0;
}


int syscall_invoke(int idx, int p0, int p1, int p2, int p3) {
	int (*handler) (int, int, int, int) = (int (*) (int, int, int, int)) syscall_handlers[idx];	

	if(unlikely(handler == NULL)) {
		errno = ENOSYS;
		return -1;
	}


#ifdef SYSCALL_DEBUG
	kprintf("syscall: pid(%d) call %s [%d] (%x, %x, %x, %x);\n", sys_getpid(), elf_symbol_lookup(handler), idx, p0, p1, p2, p3);
#endif


	register int r = handler(p0, p1, p2, p3);

#ifdef SYSCALL_DEBUG
	kprintf("% returned %x\n", r);
#endif

	return r;
}


#endif
