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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>





/*!
 * @brief Print formatted output to the debugger and halt.
 */
__attribute__((noreturn))
void kpanicf(const char* fmt, ...) {

    arch_intr_disable();


    char buf[CONFIG_BUFSIZ] = { 0 };

    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), fmt, v);
    va_end(v);


    kprintf_disable();

    int i;
    for(i = 0; buf[i]; i++)
        arch_debug_putc(buf[i]);



#if defined(DEBUG) && DEBUG_LEVEL >= 4
    //runtime_stacktrace();
#endif

    // TODO: arch_cpu_halt()
    for(;;);

}
