#include <xdev.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>
#include <xdev/syscall.h>
#include <libc.h>


#define MAX_SYSCALL			1024

typedef long (*syscall_handler_t)
		(long, long, long, long, long);


mutex_t mtx_syscall;
syscall_handler_t __handlers[MAX_SYSCALL];

extern int syscalls_start;
extern int syscalls_end;




int syscall_init(void) {
	mutex_init(&mtx_syscall, MTX_KIND_DEFAULT);

	memset(__handlers, 0, sizeof(__handlers));
	
	struct {
		int number;
		void* ptr;
	} *handler = (void*) &syscalls_start;

	for(
		handler = (void*) &syscalls_start;
		(uintptr_t) handler < (uintptr_t) &syscalls_end;
		handler++
	) syscall_register(handler->number, handler->ptr);


	return E_OK;
}


int syscall_register(int number, void* handler) {
	KASSERT(number < MAX_SYSCALL);


	__handlers[number] = (syscall_handler_t) handler;
	return E_OK;
}

int syscall_unregister(int number) {
	KASSERT(number < MAX_SYSCALL);

	__handlers[number] = (syscall_handler_t) NULL;
	return E_OK;
}


long syscall_handler(long number, long p0, long p1, long p2, long p3, long p4) {
	mutex_lock(&mtx_syscall);
	long r = __handlers[number] (p0, p1, p2, p3, p4);
	mutex_unlock(&mtx_syscall);

	return r;
}

EXPORT(syscall_register);
EXPORT(syscall_unregister);
