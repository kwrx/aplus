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
#include <limits.h>
#include <sys/types.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>


size_t strcspn(const char *str, const char* reject) {

	size_t reject_length = 0;

	while (reject[reject_length])
		reject_length++;


	for (size_t result = 0; 1; result++) {

		char c = str[result];
		if (!c)
			return result;
	
	
		int matches = 0;
		for (size_t i = 0; i < reject_length; i++) {
			
			if (str[result] != reject[i])
				continue;

			matches = 1;
			break;
		}

		if (matches)
			return result;

	}

}


TEST(libk_strcspn_test, {

	DEBUG_ASSERT(strcspn("Hello World!", " ") == 5);
	DEBUG_ASSERT(strcspn("Hello World!", "o") == 4);
	DEBUG_ASSERT(strcspn("Hello World!", "W") == 6);
	DEBUG_ASSERT(strcspn("Hello World!", "d") == 10);
	DEBUG_ASSERT(strcspn("Hello World!", "H") == 0);
	DEBUG_ASSERT(strcspn("Hello World!", "l") == 2);
	DEBUG_ASSERT(strcspn("Hello World!", "r") == 8);
	DEBUG_ASSERT(strcspn("Hello World!", "e") == 1);
	DEBUG_ASSERT(strcspn("Hello World!", "w") == 12);
	DEBUG_ASSERT(strcspn("Hello World!", "oW") == 4);
	DEBUG_ASSERT(strcspn("Hello World!", "Hl") == 0);
	DEBUG_ASSERT(strcspn("Hello World!", "Wd") == 6);
	DEBUG_ASSERT(strcspn("Hello World!", "HlW") == 0);
	DEBUG_ASSERT(strcspn("Hello World!", "HlWd") == 0);

	DEBUG_ASSERT(strcspn("Hello World!", "Helo Wrd!") == 0);

});