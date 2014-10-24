//
//  bufio.h
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

#ifndef _BUFIO_H
#define _BUFIO_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>


typedef struct bufio {
	void* raw;
	size_t size;
	off_t offset;
	uint32_t type;
	spinlock_t lock;
	task_t* task;
} bufio_t;

#endif
