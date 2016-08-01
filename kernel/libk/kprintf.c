#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/ipc.h>
#include <xdev/mm.h>
#include <libc.h>

#if DEBUG
mutex_t mtx_kprintf = MTX_INIT(MTX_KIND_DEFAULT, NULL);


int
kprintf(int flags, const char *fmt, ...) {

	KASSERT(flags < 16);

	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	int out = vsprintf(buf, fmt, args);
	
	
	mutex_lock(&mtx_kprintf);

	int i;
	for(i = 0; i < out; i++)
		debug_send(buf[i], flags);

	mutex_unlock(&mtx_kprintf);


	
	va_end(args);
	return out;
}

int
std_kprintf(const char *fmt, ...) {

	char buf[1024] = {0};
	va_list args;
	va_start(args, fmt);
	int out = vsprintf(buf, fmt, args);
	
	
	mutex_lock(&mtx_kprintf);

	int i;
	for(i = 0; i < out; i++)
		debug_send(buf[i], LOG);

	mutex_unlock(&mtx_kprintf);



	va_end(args);
	return out;
}

EXPORT(kprintf);

#endif
