#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <xdev/timer.h>
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
