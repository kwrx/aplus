#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <libc.h>


#define MAX_SYSCALL			1024

typedef long (*syscall_handler_t)
		(long, long, long, long, long);


static mutex_t mtx_syscall;
static syscall_handler_t __handlers[MAX_SYSCALL];
#if CONFIG_SYSCALL_DEBUG
static char* __handlers_name[MAX_SYSCALL];
#endif

extern int syscalls_start;
extern int syscalls_end;




int syscall_init(void) {
	mutex_init(&mtx_syscall, MTX_KIND_DEFAULT, "syscall");

	memset(__handlers, 0, sizeof(__handlers));
	
	struct {
		int number;
		void* ptr;
		char* name;
	} *handler = (void*) &syscalls_start;

	for(
		handler = (void*) &syscalls_start;
		(uintptr_t) handler < (uintptr_t) &syscalls_end;
		handler++
	) {
		syscall_register(handler->number, handler->ptr);
#if CONFIG_SYSCALL_DEBUG
		__handlers_name[handler->number] = handler->name;
#endif
	}


	return E_OK;
}


int syscall_register(int number, void* handler) {
	KASSERT(number < MAX_SYSCALL);
	KASSERTF(!__handlers[number], "%d", number);

	__handlers[number] = (syscall_handler_t) handler;
	return E_OK;
}

int syscall_unregister(int number) {
	KASSERT(number < MAX_SYSCALL);

	__handlers[number] = (syscall_handler_t) NULL;
	return E_OK;
}


long syscall_handler(long number, long p0, long p1, long p2, long p3, long p4) {
	KASSERTF(__handlers[number], "%d", number);
	
#if CONFIG_SYSCALL_DEBUG
	kprintf(LOG, "syscall(%d): %s (%p, %p, %p, %p, %p)\n", number, __handlers_name[number], p0, p1, p2, p3, p4);
#endif

	mutex_lock(&mtx_syscall);
	long r = __handlers[number] (p0, p1, p2, p3, p4);
	mutex_unlock(&mtx_syscall);

	return r;
}

EXPORT(syscall_register);
EXPORT(syscall_unregister);
