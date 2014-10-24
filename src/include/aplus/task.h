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

#ifndef _TASK_H
#define _TASK_H

#include <stdint.h>
#include <aplus.h>
#include <aplus/fs.h>

#include <signal.h>
#include <sys/types.h>

#include <setjmp.h>


#define TASK_MAX_FD					512
#define TASK_STACKSIZE				0x4000

#define TASK_STATE_ALIVE			0
#define TASK_STATE_ZOMBIE			1
#define TASK_STATE_DEAD				2

#define TASK_PRIORITY_MIN			1
#define TASK_PRIORITY_LOW			5
#define TASK_PRIORITY_REGULAR		10
#define TASK_PRIORITY_HIGH			20
#define TASK_PRIORITY_MAX			30


#define UID_SUPERUSER				1000
#define GID_SUPERUSER				1000


#define TASK_STACKADDR				0xF0000000



typedef struct task_env {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t eax;
	uint32_t edx;
	uint32_t eflags;
	uint32_t ebp;
	uint32_t eip;
} __attribute__ ((packed)) task_env_t;


typedef struct task {
	pid_t pid;

	struct {
		task_env_t* env;
		uint32_t stack;
		uint32_t cr3;
	} context;


	uid_t uid;
	gid_t gid;
	
	inode_t* cwd;
	inode_t* exe;
	inode_t* fd[TASK_MAX_FD];
	
	struct {
		uint32_t vaddr;
		uint32_t ptr;
		uint32_t length;
	} image;
	
	void (*signal_handler) (int);
	int signal_sig;
	
	
	uint32_t state;
	uint32_t priority;
	uint32_t clock;
	int exitcode;
	
	
	struct task* parent;
} task_t;


#endif
