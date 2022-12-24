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


int memcmp(const void* va, const void* vb, size_t size) {

    const unsigned char* a = va;
    const unsigned char* b = vb;

    for(size_t i = 0; i < size; i++) {

        if(*a != *b)
            return *a - *b;

        a++;
        b++;

    }

    return 0;

}


TEST(libk_memcmp_test, {

    char a[] = "Hello World!";
    char b[] = "Hello World!";

    DEBUG_ASSERT(memcmp(a, b, sizeof(a)) == 0);

});