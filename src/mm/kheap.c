//
//  kheap.c
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <aplus/mm.h>
#include <grub.h>


#define BITMAP_SET(bmp, bit)	\
	bmp[bit >> 5] |= (1 << (bit % 32))
	
#define BITMAP_CLR(bmp, bit)	\
	bmp[bit >> 5] &= ~(1 << (bit % 32))
	
#define BITMAP_TST(bmp, bit)	\
	(bmp[bit >> 5] & (1 << (bit % 32)))
	
	
#define GETBIT(addr)	\
	((uint32_t) addr / BLKSIZE)




static heap_t kheap;
extern uint64_t memsize;


static uint8_t __bitmap[131072];


static int bitmap_first(heap_t* heap, size_t size) {
	if(size == 0)
		return -1;
	

	int hsize = heap->size / BLKSIZE;
	int hused = heap->used / BLKSIZE;


	if(size > hsize)
		return -1;

	if(size >= (hsize - hused))
		return -1;


	for(register int i = 0; i < hsize; i++) {

		if(heap->bitmap[i] == 0xFFFFFFFF)
			continue;
	
		for(register int j = 0, f = 0; j < size; j++) {
			if(BITMAP_TST(heap->bitmap, (i + j)))
				continue;
			
			f++;	
			if(f == size)
				return i;
		}
	}
	
	return -1;
}



void* bitmap_alloc(heap_t* heap, size_t size) {
	if(!heap)
		return NULL;
		
	if(!heap->bitmap)
		return NULL;
		
	if(heap->used >= heap->size)
		return NULL;

	if(!size)
		return NULL;
		
	
	size /= BLKSIZE;
	size += 1;
	

	int index = bitmap_first(heap, size);
	if(index == -1)
		return NULL;
				
	for(int i = 0; i < size; i++)
		BITMAP_SET(heap->bitmap, (index + i));
	
	
	heap->used += (size * BLKSIZE);
	
	return (void*) (index * BLKSIZE);
}

void bitmap_free(heap_t* heap, void* addr, size_t size) {
	if(!heap)
		return;
		
	if(!heap->bitmap)
		return;
		

	if(size % BLKSIZE) {
		size /= BLKSIZE;
		size += 1;
	}else {
		size /= BLKSIZE;
	}
	
	int index = GETBIT(addr);
	for(int i = 0; i < size; i++)
		BITMAP_CLR(heap->bitmap, (index + i));
		
		
	heap->used -= (size * BLKSIZE);
}



int kheap_init() {

	kheap.bitmap = (uint32_t*) __bitmap;
	kheap.size = memsize;
	kheap.alloc = bitmap_alloc;
	kheap.free = bitmap_free;
	
	memset(kheap.bitmap, 0, (kheap.size / BLKSIZE));
	mm_setheap(&kheap);
	

	/* Preserve Kernel Reserved Memory & initrd*/
	halloc(&kheap, ((uint32_t*) mbd->mods_addr) [1]);

	return 0;
}
