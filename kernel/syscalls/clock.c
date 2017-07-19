#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/timer.h>
#include <libc.h>

SYSCALL(18, clock_gettime,
int sys_clock_gettime(clockid_t id, struct timespec *tv) {
	if(unlikely(!tv)) {
		errno = EINVAL;
		return -1;
	}

	switch(id) {
		case CLOCK_REALTIME:
			tv->tv_sec = timer_gettimestamp();
			tv->tv_nsec = (timer_getus() % 1000000) * 1000;
			break;
		case CLOCK_MONOTONIC:
			tv->tv_sec = timer_getus() / 1000000;
			tv->tv_nsec = (timer_getus() % 1000000) * 1000;
			break;
		case CLOCK_PROCESS_CPUTIME_ID:
			tv->tv_sec = (current_task->clock.tms_utime + current_task->clock.tms_cutime) / CLOCKS_PER_SEC;
			tv->tv_nsec = (current_task->clock.tms_utime + current_task->clock.tms_cutime) * (1000000000 / CLOCKS_PER_SEC);
			break;
		case CLOCK_THREAD_CPUTIME_ID:
			tv->tv_sec = (current_task->clock.tms_utime) / CLOCKS_PER_SEC;
			tv->tv_nsec = (current_task->clock.tms_utime) * (1000000000 / CLOCKS_PER_SEC);
			break;
		default:
			errno = EINVAL;
			return -1;
	}

	return 0;
});

SYSCALL(53, clock_getres,
int sys_clock_getres(clockid_t id, struct timespec *tv) {
	if(unlikely(!tv)) {
		errno = EINVAL;
		return -1;
	}

	tv->tv_sec = 0;
	tv->tv_nsec = (1000000000 / CLOCKS_PER_SEC);

	return 0;
});
