#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/timer.h>
#include <libc.h>

SYSCALL(18, gettimeofday,
int sys_gettimeofday(struct timeval *tv, struct timezone *tz) {
	(void) tz;

	if(unlikely(!tv)) {
		errno = EINVAL;
		return -1;
	}

	tv->tv_sec = timer_gettime();
	tv->tv_usec = timer_getms() * 1000;

	return 0;
});
