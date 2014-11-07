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


task_t* current_task;
task_t* kernel_task;

list_t* task_queue;

static int sched_enabled = 0;


int im_superuser() {
	if(!current_task)
		return -1;
		
	if(current_task->uid == UID_SUPERUSER && current_task->gid == GID_SUPERUSER)
		return 1;
	else
		return 0;
}

void schedule_enable() {
	sched_enabled = 1;
}

void schedule_disable() {
	sched_enabled = 0;
}

pid_t schedule_nextpid() {
	static pid_t nextpid = 0;
	return nextpid++;
}

int schedule_init() {
	list_init(task_queue);
	task_init();
	
	
	schedule_enable();
	return 0;
}



static task_t* schedule_next() {
	
	task_t* newtask = current_task;
	
	do {
		newtask = (task_t*) list_next(task_queue, (listval_t) newtask);
		if(!newtask)
			newtask = (task_t*) list_head(task_queue);
		
	} while(newtask->state != TASK_STATE_ALIVE);

	
	return newtask;
}

void schedule() {
	if(sched_enabled == 0)
		return;
		
	if(list_empty(task_queue))
		return;

		
	current_task->clock += 1;
	
	if(current_task->clock % current_task->priority)
		return;
		
	task_switch(schedule_next());
}

void schedule_yield() {
	if(sched_enabled == 0)
		return;
		
	if(list_empty(task_queue))
		return;
		
	task_switch(schedule_next());
}

void schedule_setpriority(int priority) {
	if(!current_task)
		return;
		
	current_task->priority = priority;
}

int schedule_wait(task_t* child) {
	if(!child)
		return -1;
		
	spinlock_waiton(child->state != TASK_STATE_DEAD);
	return child->exitcode;
}

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

void schedule_exit2(task_t* task, int status) {
	if(!task)
		return;
		
	list_remove(task_queue, (listval_t) task);
		
	task->state = TASK_STATE_DEAD;
	task->exitcode = status;
	
	kfree((void*) task->image.ptr);
	schedule_yield();
}

void schedule_exit(int status) {
	schedule_exit2(current_task, status);
}

pid_t schedule_getpid() {
	if(!current_task)
		return -1;
		
	return current_task->pid;
}

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

void* schedule_sbrk(ptrdiff_t increment) {
	if(!current_task)
		return NULL;
		
	if(current_task->image.vaddr == 0)
		return NULL;
		
	if(increment == 0)
		return (void*) ((uint32_t) current_task->image.vaddr + current_task->image.length);

	current_task->image.length += increment;
	current_task->image.ptr = (uint32_t) krealloc((void*) current_task->image.ptr, current_task->image.length);
	current_task->image.vaddr = vmm_map(current_task->context.cr3, mm_paddr((void*) current_task->image.ptr), current_task->image.vaddr, current_task->image.length, VMM_FLAGS_DEFAULT);
	
	return (void*) ((uint32_t) current_task->image.vaddr + current_task->image.length);
}

