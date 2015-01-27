#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/times.h>
#include <time.h>

extern task_t* current_task;

clock_t sys_times(struct tms* tm) {
	if(tm == NULL) 
		return (clock_t) current_task->clock;
	

	tm->tms_utime = current_task->clock;
	tm->tms_stime = 0;
	tm->tms_cstime = 0;

	
	task_t* child = (task_t*) schedule_child();
	if(child)
		tm->tms_cutime = current_task->clock + child->clock;
	 else 
		tm->tms_cutime = 0;


	return (clock_t) current_task->clock;
}

SYSCALL(sys_times, 14);
