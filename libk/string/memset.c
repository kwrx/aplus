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


void* memset(void* dest, int c, size_t n) {

	for(int i = 0; i < n; i++)
		((char*) dest) [i] = (char) c;

    return dest;
    
}


TEST(libk_memset_test, {

	char a[] = "Hello World!";
	char b[] = "Hello World!";

	memset(a, 'A', 5);
	memset(b, 'A', 5);

	DEBUG_ASSERT(strcmp(a, b) == 0);

});