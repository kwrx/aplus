//
//  mm.h
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

#ifndef _MM_H
#define _MM_H

#include <stdint.h>
#include <aplus.h>
#include <aplus/task.h>


/*
 * 0x00000000 - 0x00800000:		Kernel Reserved
 * 0x00800000 - 0x00A00000:		Not available
 * 0x00A00000 - 0x40000000: 	User executable
 * 0x40000000 - 0xE0000000:		Kernel Heap
 * 0xE0000000 - 0xF0000000;		Linear Frame Buffer (User)
 * 0xF0000000 - 0xFFFFFFFF;		Kernel Heap
 */

#define MM_VBASE			0x40000000
#define MM_VSIZE			(0xFFFFFFFF - MM_VBASE)

#define MM_LBASE			0x00000000
#define MM_LSIZE			0x00800000

#define MM_UBASE			0x00A00000
#define MM_USIZE			(MM_VBASE - MM_UBASE)



#define VMM_FLAGS_PRESENT	0x01
#define VMM_FLAGS_RDWR		0x02
#define VMM_FLAGS_USER		0x04
#define VMM_FLAGS_DEFAULT	(VMM_FLAGS_PRESENT | VMM_FLAGS_RDWR)

#define VMM_MAX_MEMORY		(0xFFFFFFFF - MM_VBASE)


#define BLKSIZE				0x1000
#define BLKMAGIC			0x1234



typedef struct heap {
	uint32_t* bitmap;
	uint32_t size;
	uint32_t used;
	
	void* (*alloc) (struct heap*, size_t);
	void (*free) (struct heap*, void*, size_t);
} heap_t;

static inline void* mm_paddr(void* vaddr) {
	if((uint32_t) vaddr > MM_VBASE)
		vaddr = (void*) ((uint32_t) vaddr - MM_VBASE);
		
	return vaddr;
}

static inline void* mm_vaddr(void* paddr) {
	if((uint32_t) paddr < MM_VBASE)
		paddr = (void*) ((uint32_t) paddr + MM_VBASE);
		
	return paddr;
}

static inline void* mm_align(void* vaddr) {
	return (void*) ((uint32_t) vaddr & ~0xFFF);
}



#ifdef CHUNKS_CHECKING
void* __kmalloc(size_t, char*, int);
#define kmalloc(s)	__kmalloc(s, __FILE__, __LINE__)
#else
void* kmalloc(size_t);
#endif


void kfree(void*);
void* krealloc(void*, size_t);

#endif
