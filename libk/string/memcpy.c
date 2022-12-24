/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aplus.                                          
 *                                                                      
 * aplus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aplus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


void *memcpy(void *restrict dest, const void *restrict src, size_t n) {

	uintptr_t d = (uintptr_t) dest;
	uintptr_t s = (uintptr_t) src;


	size_t i = 0;

	for(; i + 64 < n; i += 64) {

		*((uint64_t*) (d + 0))  = *((uint64_t*) (s + 0));
		*((uint64_t*) (d + 8))  = *((uint64_t*) (s + 8));
		*((uint64_t*) (d + 16)) = *((uint64_t*) (s + 16));
		*((uint64_t*) (d + 24)) = *((uint64_t*) (s + 24));
		*((uint64_t*) (d + 32)) = *((uint64_t*) (s + 32));
		*((uint64_t*) (d + 40)) = *((uint64_t*) (s + 40));
		*((uint64_t*) (d + 48)) = *((uint64_t*) (s + 48));
		*((uint64_t*) (d + 56)) = *((uint64_t*) (s + 56));

		d += 64;
		s += 64;

	}

	for(; i + 32 < n; i += 32) {

		*((uint64_t*) (d + 0))  = *((uint64_t*) (s + 0));
		*((uint64_t*) (d + 8))  = *((uint64_t*) (s + 8));
		*((uint64_t*) (d + 16)) = *((uint64_t*) (s + 16));
		*((uint64_t*) (d + 24)) = *((uint64_t*) (s + 24));

		d += 32;
		s += 32;

	}

	for(; i + 16 < n; i += 16) {

		*((uint64_t*) (d + 0)) = *((uint64_t*) (s + 0));
		*((uint64_t*) (d + 8)) = *((uint64_t*) (s + 8));

		d += 16;
		s += 16;

	}

	for(; i + 8 < n; i += 8) {
	
		*((uint64_t*) (d)) = *((uint64_t*) (s));

		d += 8;
		s += 8;

	}

	for(; i + 4 < n; i += 4) {
	
		*((uint32_t*) (d)) = *((uint32_t*) (s));

		d += 4;
		s += 4;

	}

	for(; i + 2 < n; i += 2) {
	
		*((uint16_t*) (d)) = *((uint16_t*) (s));

		d += 2;
		s += 2;

	}

	for(; i < n; i += 1) {
	
		*((uint8_t*) (d)) = *((uint8_t*) (s));

		d += 1;
		s += 1;

	}

	return dest;
	
}


TEST(libk_memcpy_test, {

	char a[] = "Hello World!";
	char b[12];

	DEBUG_ASSERT(memcpy(b, a, sizeof(b)) == b);
	DEBUG_ASSERT(memcmp(a, b, sizeof(b)) == 0);

});