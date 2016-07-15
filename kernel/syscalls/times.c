#include <xdev.h>
#include <xdev/syscall.h>
#include <xdev/debug.h>
#include <xdev/task.h>
#include <libc.h>


SYSCALL(14, times,
clock_t sys_times(struct tms* tms) {
	if(!tms)
		return current_task->clock;


	tms->tms_utime = current_task->clock;
	tms->tms_stime = 0;
	tms->tms_cutime = 0;
	tms->tms_cstime = 0;

	return tms->tms_utime;
});
