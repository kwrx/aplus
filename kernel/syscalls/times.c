#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>


SYSCALL(14, times,
clock_t sys_times(struct tms* tms) {
	if(!tms)
		return timer_getticks();


	memcpy(tms, &current_task->clock, sizeof(struct tms));
	return timer_getticks();
});
