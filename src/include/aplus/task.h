//
//  task.h
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


#include <stdint.h>
#include <sys/times.h>
#include <aplus/vfs.h>

#define TASK_MAX_FD		512
#define TASK_STACKSIZE	4096

#define TASK_STATE_RUNNING		0
#define TASK_STATE_IDLE			1
#define TASK_STATE_ZOMBIE		2
#define TASK_STATE_KILLED		3


#define TASK_PRIORITY_MIN		5
#define TASK_PRIORITY_LOW		10
#define TASK_PRIORITY_NORMAL	25
#define TASK_PRIORITY_HIGH		50
#define TASK_PRIORITY_MAX		100


#define VIRTUAL_CODE_ADDRESS	0xC0000000
#define VIRTUAL_STACK_ADDRESS	0xF0000000

#define VIRTUAL_STACK_LENGTH	TASK_STACKSIZE


typedef struct task {
	uint32_t pid;
	uint32_t stack;

	uint64_t clock;
	uint32_t state;
	uint32_t priority;
	uint32_t exitcode;
	
	void* vmm;
	
	void* image;
	uint32_t imagelen;

	char** argv;
	char** environ;
	
	uint32_t fd[TASK_MAX_FD];
	inode_t* cwd;
	inode_t* exe;
	
	void (*signal_handler) (int sig);
	int signal_sig;
	
	struct task* prev;
	struct task* next;
	struct task* parent;
} task_t;
