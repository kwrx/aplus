#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <libc.h>





volatile task_t* current_task = NULL;
volatile task_t* kernel_task = NULL;
volatile task_t* task_queue = NULL;

static ktime_t __last_scheduling = 0;

#define __timing(t)												\
	register ktime_t t = timer_getus() - __last_scheduling;		\
	__last_scheduling = timer_getus();							\




static void sched_next(void) {
	do {
		current_task = current_task->next;
		if(unlikely(!current_task))
			current_task = task_queue;

		KASSERT(current_task);
	} while(current_task->status != TASK_STATUS_READY);
}




pid_t sched_nextpid() {
	static pid_t nextpid = 1;
	return nextpid++;
}



void schedule(void) {
	if(unlikely(!current_task))
		return;



	__timing(t);

	current_task->clock.tms_utime += t;
	if(likely(current_task->parent))
		current_task->parent->clock.tms_cutime += t;
		
	if(likely(((int)current_task->clock.tms_utime / 1000) % ((int)((20 - current_task->priority) + 1))))
		goto nosched;

	if(likely(current_task->status == TASK_STATUS_RUNNING))
		current_task->status = TASK_STATUS_READY;


	volatile task_t* prev_task = current_task;
	sched_next();

	
	current_task->status = TASK_STATUS_RUNNING;
	arch_task_switch(prev_task, current_task);

nosched:
	return;
}

void schedule_yield(void) {
	if(unlikely(!current_task))
		return;


	__timing(t);

	current_task->clock.tms_utime += t;
	if(likely(current_task->parent))
		current_task->parent->clock.tms_cutime += t;

	if(likely(current_task->status == TASK_STATUS_RUNNING))
		current_task->status = TASK_STATUS_READY;


	volatile task_t* prev_task = current_task;
	sched_next();
	

	current_task->status = TASK_STATUS_RUNNING;
	arch_task_switch(prev_task, current_task);
}


EXPORT(current_task);
EXPORT(kernel_task);
EXPORT(task_queue);
