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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>



char* strstr(const char* haystack, const char* needle) {

    if(!needle[0]) {
        return (char*) haystack;
    }

	for (size_t i = 0; haystack[i]; i++) {

        int diff = 0;
        
        for(size_t j = 0; needle[j]; j++) {

            if(haystack[i + j] == needle[j])
                continue;

            diff = 1;
            break;

        }

        if(diff)
            continue;
		
		return (char*) haystack + i;
	}

	return NULL;
}


TEST(libk_strstr_test, {

    char str[] = "Hello World!";

    DEBUG_ASSERT(strstr(str, "Hello") == str);
    DEBUG_ASSERT(strstr(str, "World") == str + 6);
    DEBUG_ASSERT(strstr(str, "World!") == str + 6);
    DEBUG_ASSERT(strstr(str, "World!!") == NULL);

});