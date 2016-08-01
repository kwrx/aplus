#include <xdev.h>
#include <xdev/ipc.h>
#include <xdev/debug.h>
#include <xdev/task.h>
#include <xdev/mm.h>
#include <libc.h>





volatile task_t* current_task = NULL;
volatile task_t* kernel_task = NULL;
volatile task_t* task_queue = NULL;

mutex_t mtx_schedule = MTX_INIT(MTX_KIND_DEFAULT, "schedule");


static void sched_next(void) {
	do {
		current_task = current_task->next;
		if(unlikely(!current_task))
			current_task = task_queue;

		KASSERT(current_task);
	} while(current_task->status != TASK_STATUS_READY);
}



pid_t sched_nextpid() {
	static pid_t nextpid = 0;
	return nextpid++;
}



void schedule(void) {
	if(unlikely(!current_task))
		return;


	mutex_lock(&mtx_schedule);

	
	current_task->clock += 1;
	if(likely((int)current_task->clock % (int)((20 - current_task->priority) + 1)))
		goto nosched;

	if(likely(current_task->status == TASK_STATUS_RUNNING))
		current_task->status = TASK_STATUS_READY;


	volatile task_t* prev_task = current_task;
	sched_next();

	
	arch_task_switch(prev_task, current_task);
	current_task->status = TASK_STATUS_RUNNING;

nosched:

	mutex_unlock(&mtx_schedule);
	return;	
}

void schedule_yield(void) {
	if(unlikely(!current_task))
		return;


	mutex_lock(&mtx_schedule);


	if(likely(current_task->status == TASK_STATUS_RUNNING))
		current_task->status = TASK_STATUS_READY;


	volatile task_t* prev_task = current_task;
	sched_next();
	
	arch_task_switch(prev_task, current_task);
	current_task->status = TASK_STATUS_RUNNING;


	mutex_unlock(&mtx_schedule);
	return;
}


EXPORT(current_task);
EXPORT(kernel_task);
EXPORT(task_queue);
