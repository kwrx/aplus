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

#include <hal/debug.h>



/*!
 * @brief Print formatted output to the debugger.
 */
void kprintf(const char* fmt, ...) {

    static char buf[8192];
    static spinlock_t buflock = SPINLOCK_INIT;

    __lock(&buflock, {

        va_list v;
        va_start(v, fmt);
        vsnprintf(buf, sizeof(buf), fmt, v);
        va_end(v);


        int i;
        for(i = 0; buf[i]; i++) {
    
            arch_debug_putc(buf[i]);

            //if(unlikely(buf[i] == '\n')) // FIXME: DEADLOCK
            //    kprintf("[%d.%d] ", core->bsp.ticks.tv_sec, (core->bsp.ticks.tv_nsec / 1000000));

        }
    });

}
