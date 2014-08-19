
//
//  panic.c
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

#define DEBUG
#include <aplus.h>
#include <aplus/task.h>
#include <aplus/vfs.h>

extern task_t* current_task;
extern task_t* kernel_task;

void __panic(char* msg, char* source, char* func, int line) {
	video_init();
	

	int pid = -1;
	if(current_task)
		pid = current_task->pid;
		
		
	char* name;
	if(current_task->exe)
		name = current_task->exe->name;

	kprintf("Panic: %s (%s[%d]: %s) from %d (%s)\n", msg, source, line, func, pid, name);
	
	sched_disable();
	__asm__ __volatile__ ("cli");
	
	if(!current_task)
		for(;;);
		
	if(current_task == kernel_task) {
		kprintf("\n[KERNEL PANIC]\n");
		for(;;);	
	}

	__asm__ __volatile__ ("sti");
	sched_enable();
	task_exit(-1);
}