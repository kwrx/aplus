//
//  sched.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>
#include <aplus/list.h>

#include <stdint.h>
#include <sys/types.h>

/**
 *	\brief Current task address
 */
task_t* current_task;

/**
 *	\brief Kernel task address
 */
task_t* kernel_task;


/**
 *	\brief List of all Task
 */
list_t* task_queue;


/**
 *	\brief Scheduling enabled status
 */
static int sched_enabled = 0;

/**
 *	\brief Get root permissions for current task.
 * 	\return true or false.
 */
int im_superuser() {
	if(!current_task)
		return -1;

	if(current_task == kernel_task)
		return 1;
		
	if(current_task->uid == UID_SUPERUSER && current_task->gid == GID_SUPERUSER)
		return 1;
	else
		return 0;
}


/**
 *	\brief Enable scheduling.
 */
void schedule_enable() {
	sched_enabled = 1;
}

/**
 *	\brief Disable scheduling.
 */
void schedule_disable() {
	sched_enabled = 0;
}

/**
 *	\brief Get a new Process ID.
 */
pid_t schedule_nextpid() {
	static pid_t nextpid = 0;
	return nextpid++;
}



/**
 *	\brief Initialize scheduling.
 *	\return success of initilization.
 */
int schedule_init() {
	list_init(task_queue);
	task_init();
	
	
	schedule_enable();
	return 0;
}


/**
 *	\brief Get next ready task.
 *	\return A ready task to schedule.
 */
static task_t* schedule_next() {
	
	task_t* newtask = current_task;
	
	do {
		newtask = (task_t*) list_next(task_queue, (listval_t) newtask);
		if(!newtask)
			newtask = (task_t*) list_head(task_queue);
		
	} while(newtask->state != TASK_STATE_ALIVE);

	
	return newtask;
}


/**
 * 	\brief Send a signal to task.
 * 	\param task Pointer of Task.
 * 	\param sig Signal to send.
 */
void schedule_signal(task_t* task, int sig) {
	if(!task)
		return;

	task->signal_sig = 0;
	if(!task->signal_handler)
		return;

	task_switch_ack();
	task->signal_handler(sig);
}


/**
 *	\brief Perform a scheduling and check TTL (Time To Live) for current task.
 */
void schedule() {
	if(sched_enabled == 0)
		return;
		
	if(list_empty(task_queue))
		return;


	if(current_task->signal_sig)
		schedule_signal(current_task, current_task->signal_sig);

	current_task->clock += 1;
	
	if(current_task->clock % current_task->priority)
		return;

	
	task_switch(schedule_next());
}


/**
 *	\brief Perform a forced scheduling.
 */
void schedule_yield() {
	if(sched_enabled == 0)
		return;
		
	if(list_empty(task_queue))
		return;
		
	task_switch(schedule_next());
}


/**
 *	\brief Set TTL (Time To Live) for current task.
 *	\param priority TTL for current task.
 */
void schedule_setpriority(int priority) {
	if(!current_task)
		return;
		
	current_task->priority = priority;
}


/**
 *	\brief Wait for child.
 *	\param child Task to wait.
 *	\return Exit value of child or -1 in case of error.
 */
int schedule_wait(task_t* child) {
	if(!child)
		return -1;
		
	spinlock_waiton(child->state != TASK_STATE_DEAD);
	return child->exitcode;
}

/**
 *	\brief Get Child task for current task.
 *	\return child task or NULL in case of error.
 */
task_t* schedule_child() {
	if(!current_task)
		return NULL;
		

	list_foreach(value, task_queue) {
		task_t* child = (task_t*) value;
		
		if(child->parent == current_task)
			return child;
	}
	
	return NULL;
}


/**
 *	\brief Release allocated resources from task.
 * 	\param task Pointer to task structure.
 */
void schedule_release(task_t* task) {
	if(!task)
		return;

#ifdef DEBUG
	void* x = kmalloc(1000);
	heap_t* h = (heap_t*) mm_getheap();
	int prevmm = h->used;
#endif

	if(task->argv) {
		for(int i = 0; task->argv[i]; i++)
			kfree(task->argv[i]);
	
		kfree(task->argv);
	}

	if(task->envp) {
		for(int i = 0; task->envp[i]; i++)
			kfree(task->envp[i]);

		kfree(task->envp);
	}

	if(task->image.ptr)
		kfree((void*) task->image.ptr);

	if(task->image.vaddr && task->image.length && task->context.cr3)
		vmm_free(task->context.cr3, task->image.vaddr, task->image.length);


#ifdef DEBUG
	kfree(x);
	kprintf("task: realesed memory for %d Bytes\n", prevmm - h->used);
#endif
}

/**
 *	\brief Terminate a task with an Exit Value.
 *	\param task Task to close.
 *	\param status Exit Value.
 */
void schedule_exit2(task_t* task, int status) {
	if(!task)
		return;
		
	list_remove(task_queue, (listval_t) task);
		
	task->state = TASK_STATE_DEAD;
	task->exitcode = status;
	
	if(task->context.owner != task->pid)
		task->context.cr3 = 0;
		
	schedule_release(task);
}


/**
 *	\brief Terminate current task.
 *	\param status Exit Value.
 * 	\see schedule_exit2
 */
void schedule_exit(int status) {
	schedule_exit2(current_task, status);
	schedule_yield();
	
	for(;;);
}


/**
 *	\brief Get Process ID of current task.
 *	\return Process ID or -1 in case of error.
 */
pid_t schedule_getpid() {
	if(!current_task)
		return -1;
		
	return current_task->pid;
}


/**
 *	\brief Get task from his Process ID.
 *	\return Task or NULL in case of error.
 */
task_t* schedule_getbypid(pid_t pid) {
	if(current_task->pid == pid)
		return current_task;
		
	list_foreach(value, task_queue) {
		task_t* t = (task_t*) value;
		
		if(t->pid == pid)
			return t;
	}
	
	return NULL;
}

/**
 *	\brief Increment address space of current task.
 *	\param increment Increment in Bytes.
 *	\return Current size of address space or NULL in case of error.
 */
void* schedule_sbrk(ptrdiff_t increment) {
	if(!current_task)
		return NULL;
		
	if(current_task->image.vaddr == 0)
		return NULL;



	void* brk = (void*) ((uint32_t) current_task->image.vaddr + current_task->image.length);		
	if(increment == 0)
		return brk;


	increment = (increment & ~0xFFF) + 0x1000;

	if(increment > 0)
		vmm_alloc(current_task->context.cr3, current_task->image.vaddr + current_task->image.length, increment, VMM_FLAGS_DEFAULT | VMM_FLAGS_USER);
	else
		vmm_free(current_task->context.cr3, current_task->image.vaddr + current_task->image.length, increment);	

	current_task->image.length += increment;

	return brk;
}

