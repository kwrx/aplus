#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>


SYSCALL(14, times,
clock_t sys_times(struct tms* tms) {
	if(!tms)
		return timer_getticks();


	memcpy(tms, &current_task->clock, sizeof(struct tms));
	return timer_getticks();
});
