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

#include <aplus.h>
#include <aplus/mm.h>

#define BITMAP_SET(bmp, bit)	\
	bmp[bit >> 5] |= (1 << (bit % 32))
	
#define BITMAP_CLR(bmp, bit)	\
	bmp[bit >> 5] &= ~(1 << (bit % 32))
	
#define BITMAP_TST(bmp, bit)	\
	(bmp[bit >> 5] & (1 << (bit % 32)))
	
	
#define GETBIT(addr)	\
	((uint32_t) addr / BLKSIZE)




heap_t kheap;
extern uint32_t memsize;
extern uint32_t kernel_low_area_size;


static int bitmap_first(heap_t* heap, size_t size) {
	if(unlikely(size == 0))
		return -1;
	

	int hsize = heap->size / BLKSIZE;
	int hused = heap->used / BLKSIZE;


	if(unlikely(size > hsize))
		return -1;

	if(unlikely(size >= (hsize - hused)))
		return -1;


	for(register int i = 0; i < hsize; i++) {

		if(unlikely(heap->bitmap[i] == 0xFFFFFFFF))
			continue;
	
		for(register int j = 0, f = 0; j < size; j++) {
			if(BITMAP_TST(heap->bitmap, (i + j)))
				continue;
			
			f++;	
			if(unlikely(f == size))
				return i;
		}
	}
	
	return -1;
}



void* bitmap_alloc(heap_t* heap, size_t size) {
	if(unlikely(!heap))
		return NULL;

	
	if(unlikely(!heap->bitmap))
		return NULL;

		
	if(unlikely(heap->used >= heap->size))
		return NULL;

	if(unlikely(!size))
		return NULL;
		
	
	size /= BLKSIZE;
	size += 1;
	

	int index = bitmap_first(heap, size);
	if(unlikely(index == -1))
		return NULL;
				
	for(int i = 0; i < size; i++)
		BITMAP_SET(heap->bitmap, (index + i));
	
	
	heap->used += (size * BLKSIZE);
	
	return (void*) (index * BLKSIZE);
}

void bitmap_free(heap_t* heap, void* addr, size_t size) {
	if(unlikely(!heap))
		return;
	
	
	if(unlikely(!heap->bitmap))
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

	
	kheap.used = 0;
	kheap.size = memsize;
	kheap.alloc = bitmap_alloc;
	kheap.free = bitmap_free;

	memset(kheap.bitmap, 0, (kheap.size / BLKSIZE));
	mm_setheap(&kheap);
	

	/* Preserve Kernel Reserved Memory & initrd*/
	halloc(&kheap, kernel_low_area_size);

	return 0;
}
