#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <libc.h>

#if DEBUG
spinlock_t lck_kprintf = SPINLOCK_UNLOCKED;


int
kprintf(const char *fmt, ...) {


	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	int out = vsprintf(buf, fmt, args);
	
	
	spinlock_lock(&lck_kprintf);

	int i;
	for(i = 0; i < out; i++)
		debug_send(buf[i]);

	spinlock_unlock(&lck_kprintf);


	
	va_end(args);
	return out;
}


EXPORT(kprintf);

#endif
