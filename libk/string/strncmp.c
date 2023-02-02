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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>


int strncmp(const char* a, const char* b, size_t n) {

	DEBUG_ASSERT(a);
	DEBUG_ASSERT(b);
	DEBUG_ASSERT(n > 0);


	for (size_t i = 0; i < n; i++) {

		unsigned char ac = (unsigned char) a[i];
		unsigned char bc = (unsigned char) b[i];

		if (ac == '\0' && bc == '\0')
			return 0;

		if (ac < bc)
			return -1;

		if (ac > bc)
			return 1;

	}

	return 0;

}


TEST(libk_strncmp_test, {

	DEBUG_ASSERT(strncmp("Hello World!", "Hello World!", 12) == 0);
	DEBUG_ASSERT(strncmp("Hello World!", "Hello World?", 11) == 0);
	DEBUG_ASSERT(strncmp("Hello World!", "Hello World?", 12) == -1);

});