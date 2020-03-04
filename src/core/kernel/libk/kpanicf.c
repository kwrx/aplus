/*                                                                      
 * GPL3 License                                                         
 *                                                                      
 * Author(s):                                                              
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 * This file is part of aPlus.                                          
 *                                                                      
 * aPlus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aPlus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/hal.h>
#include <aplus/core/ipc.h>


/*!
 * @brief Print formatted output to the debugger and halt.
 */
void kpanicf(const char* fmt, ...) {

    char buf[8192];

    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), fmt, v);
    va_end(v);


    int i;
    for(i = 0; buf[i]; i++)
        arch_debug_putc(buf[i]);


    // TODO: arch_cpu_halt()
    for(;;);

}
