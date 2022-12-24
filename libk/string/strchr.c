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
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>


char* strchrnul(const char*, int);


char* strchr(const char *s, int c) {

    char *r = strchrnul(s, c);
	return *(unsigned char *) r == (unsigned char) c ? r : 0;

}


TEST(libk_strchr_test, {

    char a[] = "Hello World!";

    DEBUG_ASSERT(strchr(a, 'H') == &a[0]);
    DEBUG_ASSERT(strchr(a, 'e') == &a[1]);
    DEBUG_ASSERT(strchr(a, 'l') == &a[2]);
    DEBUG_ASSERT(strchr(a, 'o') == &a[4]);
    DEBUG_ASSERT(strchr(a, ' ') == &a[5]);
    DEBUG_ASSERT(strchr(a, 'W') == &a[6]);
    DEBUG_ASSERT(strchr(a, 'r') == &a[8]);
    DEBUG_ASSERT(strchr(a, 'd') == &a[10]);
    DEBUG_ASSERT(strchr(a, '!') == &a[11]);
    DEBUG_ASSERT(strchr(a, 'z') == NULL);

});