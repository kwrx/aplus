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
#include <stdio.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>




/*!
 * @brief Print formatted output to the debugger and halt.
 */
__noreturn
__nosanitize("undefined")
void kpanicf(const char* fmt, ...) {

    arch_intr_disable();


    char buf[CONFIG_BUFSIZ] = { 0 };

    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), fmt, v);
    va_end(v);

    kprintf_mask(~(1 << current_cpu->id));

    arch_debug_putc('\e');
    arch_debug_putc('[');
    arch_debug_putc('3');
    arch_debug_putc('1');
    arch_debug_putc('m');

    for(size_t i = 0; buf[i]; i++) {
        arch_debug_putc(buf[i]);
    }

#if DEBUG_LEVEL_ERROR
    runtime_stacktrace();
#endif
    runtime_dump();

    arch_debug_putc('\e');
    arch_debug_putc('[');
    arch_debug_putc('0');
    arch_debug_putc('m');

    
    for(;;) {
        __cpu_pause();
        __cpu_halt();
    }

}
