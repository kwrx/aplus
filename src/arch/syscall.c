#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/syscall.h>
#include <aplus/attribute.h>

#include <errno.h>

#ifdef DEBUG
#define SYSCALL_DEBUG
#endif

static void** syscall_handlers = NULL;

int syscall_init() {
	list_t* syslist = attribute("syscall");

	syscall_handlers = (void**) kmalloc(sizeof(void*) * syslist->size);
	memset(syscall_handlers, 0, sizeof(void*) * syslist->size);

	list_foreach(value, syslist) {
		syscall_t* sys = (syscall_t*) value;

		if(syscall_handlers[sys->number])
			kprintf("syscall: duplicate number of %d handler\n");

		syscall_handlers[sys->number] = sys->handler;
	}

	kprintf("syscall: loaded %d handlers\n", syslist->size);
	list_destroy(syslist);


#ifdef DEBUG
	#define NSYSCALLS		1024

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

int syscall_invoke(int idx, int p0, int p1, int p2, int p3, int p4) {
	void* handler = syscall_handlers[idx];	

	if(handler == NULL) {
		errno = ENOSYS;
		return -1;
	}

	int r = 0;

	__asm__ (
		"push ebx			\n"
		"push ecx			\n"
		"push edx			\n"
		"push esi			\n"
		"push edi			\n"
		"call eax			\n"
		"add esp, 20		\n"
		: "=a"(r) 
		: "a"(handler), "b"(p4), "c"(p3), "d"(p2), "S"(p1), "D"(p0)
	);

	return r;
}


int syscall_handler(regs_t* r) {

#ifdef SYSCALL_DEBUG
	kprintf("syscall: %d call %d (%x, %x, %x, %x, %x);\n", sys_getpid(), r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);
#endif

	int ret = syscall_invoke(r->eax, r->ebx, r->ecx, r->edx, r->esi, r->edi);

#ifdef SYSCALL_DEBUG
	kprintf("% returned %x\n", ret);
#endif

	return ret;
}
