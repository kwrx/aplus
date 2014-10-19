//
//  heap.c
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
#include <sys/types.h>

#include <aplus/mm.h>


void* halloc(heap_t* heap, size_t size) {
	if(heap)
		if(heap->alloc)
			return heap->alloc(heap, size);
			
	return 0;
}

void hfree(heap_t* heap, void* addr, size_t size) {
	if(heap)
		if(heap->free)
			heap->free(heap, addr, size);
}




