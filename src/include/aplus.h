
//
//  aplus.h
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

#ifndef _APLUS_H
#define _APLUS_H


#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <config.h>

typedef struct bootargs {
	struct {
		uint64_t size;
		uint32_t pagesize;
		uint32_t start;
	} memory;

	struct {
		uint32_t ptr;
		uint32_t size;
	} ramdisk;

	struct {
		uint16_t width;
		uint16_t height;
		uint16_t depth;
		uint32_t pitch;
		uint32_t base;
		uint32_t size;
	} lfb;

	struct {
		char* args;
		int length;
	} cmdline;

	struct {
		uint32_t num;
		uint32_t addr;
		uint32_t size;
		uint32_t shndx;
	} exec;

	int flags;
} bootargs_t;



extern bootargs_t* mbd;



#ifndef DEBUG
#define kprintf(a, b...)
#endif


#define likely(x)			__builtin_expect(!!(x), 1)
#define unlikely(x)			__builtin_expect(!!(x), 0)

#ifndef __weak
#define __weak				__attribute__((weak))
#endif

#ifndef __packed
#define __packed			__attribute__((packed))
#endif


#endif
