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


/*
 * 0x00000000 - 0x00A00000:		Kernel Reserved
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




#define VMM_PROT_READ		1
#define VMM_PROT_WRITE		2
#define VMM_PROT_EXEC		4
#define VMM_PROT_NONE		0



#define VMM_MASK			~0xFFF
#define VMM_MAX_MEMORY		(0xFFFFFFFF - MM_VBASE)


#define BLKSIZE				0x4000
#define MM_MASK				~0xFFF

#define BLKMAGIC			0xF7A2


#define MM_ERROR			1
#define MM_OK				0

#define MMZONE_NO_BUFFER	1


typedef struct mmzone {
	void* address;
	size_t length;
	int flags;
	void* ctx;
	void* buffer;
	int (*handler) (void* request_addr);
} mmzone_t;


typedef uint32_t bmpvec_t[131072 >> 2];

typedef struct heap {
	bmpvec_t bitmap;
	uint64_t size;
	uint64_t used;
	
	void* (*alloc) (struct heap*, size_t);
	void (*free) (struct heap*, void*, size_t);
} heap_t;


static inline void* mm_align(void* vaddr) {
	return (void*) ((uint32_t) vaddr & VMM_MASK);
}



#ifdef MM_DEBUG
#define kmalloc(x)	__kmalloc(x, __FILE__, __LINE__)
void* __kmalloc(size_t, char*, int);
#else
void* kmalloc(size_t);
#endif


void kfree(void*);
void* krealloc(void*, size_t);


#endif
