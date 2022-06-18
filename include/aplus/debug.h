/*                                                                      
 * Author(s):                                                           
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
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
                                                                        
#ifndef _APLUS_DEBUG_H
#define _APLUS_DEBUG_H

#ifndef __ASSEMBLY__


#if defined(DEBUG)
#define DEBUG_ASSERT(i) {                                               \
        if(unlikely(!(i)))                                              \
            kpanicf("ERROR! Assert failed on %s() in %s:%d: '%s'\n",    \
                __func__, __FILE__, __LINE__, #i);                      \
        }

#else
#define DEBUG_ASSERT(i)     \
        (void) 0
#endif


#define PANIC_ON(i)                                                     \
    if(unlikely(!(i))) {                                                \
        kpanicf("HALT! Found a bug on %s() in %s:%d: '%s'\n",           \
            __func__, __FILE__, __LINE__, #i);                          \
    }



__BEGIN_DECLS

void kprintf(const char*, ...) __format(printf, 1, 2);
void kprintf_pause();
void kprintf_resume();
void kprintf_mask(int);

void kpanicf(const char*, ...) __format(printf, 1, 2) __noreturn;

__END_DECLS

#endif

#endif
