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

#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>


size_t strlcat(char *restrict dest, const char *restrict src, size_t n) {

    DEBUG_ASSERT(dest);
    DEBUG_ASSERT(src);
    DEBUG_ASSERT(n > 0);
    
    size_t i, j;

    for(i = 0; i < n && dest[i] != '\0'; i++)
        ;

    for(j = 0; i < n && src[j] != '\0'; i++, j++)
        dest[i] = src[j];

    dest[i] = '\0';

    return i;

}


TEST(libk_strlcat_test, {

    char a[32] = "Hello World!";
    char b[32] = "Hello World!";

    DEBUG_ASSERT(strlcat(a, "Hello World!", 12) == 12);
    DEBUG_ASSERT(strlcat(b, "Hello World!", 24) == 24);

    DEBUG_ASSERT(strcmp(a, "Hello World!") == 0);
    DEBUG_ASSERT(strcmp(b, "Hello World!Hello World!") == 0);

});