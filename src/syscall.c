#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/syscall.h>
#include <aplus/attribute.h>

#include <errno.h>


static void** syscall_handlers = NULL;

int syscall_init() {
	syscall_handlers = (void**) kmalloc(sizeof(void*) * 1024);
	memset(syscall_handlers, 0, sizeof(void*) * 1024);
	

	list_t* syslist = attribute("syscall");
	list_foreach(value, syslist) {
		syscall_t* sys = (syscall_t*) value;

		syscall_handlers[sys->number] = sys->handler;
	}
	
	list_destroy(syslist);
	return 0;
}

int syscall_invoke(void* handler, int p0, int p1, int p2, int p3, int p4) {
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
	if(r->eax > 1024) {
		errno = ENOSYS;
		return -1;
	}

	return syscall_invoke(syscall_handlers[r->eax], r->ebx, r->ecx, r->edx, r->esi, r->edi);
}
