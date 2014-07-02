
//
//  task.c
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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <stdint.h>
#include <aplus.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>

extern inode_t* fs_root;
extern void* kernel_directory;

static uint64_t next_pid = 0;

volatile task_t* current_task = 0;
volatile task_t* task_queue = 0;
volatile task_t* kernel_task = 0;



static uint8_t sched_enabled = 0;
static uint32_t time_to_sched = 0;




static void* make_stack(uint32_t eip, void* data) {
	uint32_t* stack = kmalloc(TASK_STACKSIZE) + TASK_STACKSIZE;
	*--stack = 0x0202;
	*--stack = 0x08;
	*--stack = eip;
	
	*--stack = data;
	*--stack = 0;
	*--stack = 0;
	*--stack = 0;
	*--stack = 0;
	*--stack = 0;
	*--stack = 0;
	*--stack = 0;

	*--stack = 0x10;
	*--stack = 0x10;
	*--stack = 0x10;
	*--stack = 0x10;

	return (void*) stack;
}

static void __task_idle() {
	for(;;);
}


int task_init() {
	current_task = task_queue = kernel_task = (task_t*) kmalloc(sizeof(task_t));
	memset(current_task, 0, sizeof(task_t));
	
	current_task->pid = next_pid++;
	current_task->esp = make_stack(__task_idle, 0);
	current_task->vmm = kernel_directory;
	current_task->cwd = fs_root;
	current_task->state = TASK_STATE_RUNNING;
	current_task->priority = TASK_PRIORITY_NORMAL;
	
	current_task->next = 0;
	current_task->prev = 0;
	current_task->parent = 0;

	
	sched_enable();
	
	return 0;
}


task_t* task_create_with_data(void* vmm, uint32_t eip, void* data) {
	task_t* t = kmalloc(sizeof(task_t));
	memset(t, 0, sizeof(task_t));


	t->pid = next_pid++;
	t->esp = make_stack(eip, data);
	t->cwd = current_task->cwd;
	t->state = TASK_STATE_RUNNING;
	t->priority = TASK_PRIORITY_NORMAL;
	t->signal_handler = 0;
	t->symtable = 0;
	t->argv = 0;
	t->image = 0;
	t->environ = current_task->environ;
	t->parent = current_task;
	
	for(int i = 0; i < TASK_MAX_FD; i++)
		t->fd[i] = current_task->fd[i];
		
		
	if(vmm == NULL) {
		t->vmm = mm_create_addrspace();
		mm_map_kernel(t->vmm);
	} else {
		t->vmm = vmm;
	}
	
	task_t* tmp = task_queue;
	while(tmp->next)
		tmp = tmp->next;
		
	tmp->next = t;
	t->prev = tmp;
	t->next = 0;

	return t;
}



void task_zombie() {
	if(!current_task)
		for(;;);
		
	current_task->state = TASK_STATE_ZOMBIE;
	for(;;);
}

void task_idle() {
	if(!current_task)
		__asm__ __volatile__ ("pause");
	else
		current_task->state = TASK_STATE_IDLE;
}

void task_wakeup() {
	if(!current_task)
		return;
		
	current_task->state = TASK_STATE_RUNNING;
}

void task_setpriority(int priority) {
	if(!current_task)
		return;
		
	current_task->priority = priority;
}


void sched_enable() {
	sched_enabled = 1;
}

void sched_disable() {
	sched_enabled = 0;
}


uint32_t schedule(uint32_t esp) {
	if(!sched_enabled)
		return esp;
		
	if(!current_task)
		return esp;
		
	time_to_sched += 1;
	current_task->time += 1;
		
	if(current_task->state != TASK_STATE_RUNNING)
		time_to_sched = current_task->priority;
		
	if(time_to_sched < current_task->priority)
		return esp;
		
	time_to_sched = 0;
		
	current_task->esp = esp;
	
	while(1) {
		current_task = current_task->next;
		if(!current_task)
			current_task = task_queue;
			
		if(current_task->state == TASK_STATE_ZOMBIE)
			continue;
			
		break;
	}
	
	
	vmm_switch(current_task->vmm);
	return current_task->esp;
}


task_t* task_create(void* vmm, uint32_t eip) {
	return task_create_with_data(vmm, eip, 0);
}


int task_exit2(task_t* t, int status) {
	if(!t || t == kernel_task) {
		errno = -EINVAL;
		return -1;
	}
	
	sched_disable();
	
	t->state = TASK_STATE_KILLED;

	if(t->prev)
		t->prev->next = t->next;
		
	if(t->next)
		t->next->prev = t->prev;
	
	kprintf("task %d exited with status: %d\n", t->pid, status);
	
	sched_enable();
	
	return 0;
}

int task_exit(int status) {
	return task_exit2(current_task, status);
}


task_t* task_getbypid(int pid) {
	task_t* tmp = task_queue;
	while(tmp) {
		if(tmp->pid == pid)
			return tmp;
			
		tmp = tmp->next;
	}
	
	return 0;
}


void task_wait(task_t* child) {
	if(!child)
		return;
		
	int wakeup = 0;	
	if(current_task->state == TASK_STATE_RUNNING)
		wakeup = 1;
		
	task_idle();
	while(child->state != TASK_STATE_KILLED && child->state != TASK_STATE_ZOMBIE)
		__asm__ __volatile__ ("pause");
	
	if(wakeup)
		task_wakeup();
}

void task_waitpid(int pid) {
	task_wait(task_getbypid(pid));
}


EXPORT(task_zombie);
EXPORT(task_idle);
EXPORT(task_wakeup);
EXPORT(task_waitpid);
EXPORT(task_setpriority);

EXPORT(sched_enable);
EXPORT(sched_disable);
