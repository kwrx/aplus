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
#include <string.h>
#include <aplus/core/base.h>
#include <aplus/core/multiboot.h>
#include <aplus/core/debug.h>
#include <aplus/core/memory.h>

#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>


void x86_exception_handler(interrupt_frame_t* frame) {
    
    DEBUG_ASSERT(frame);


    switch(frame->intno) {

        case 0x20 ... 0xFF:
            break;

        case 0x0E:

            // TODO: Handle Page Fault (Copy on Write, Swap, ecc...)

            kpanicf("x86-pfe: errno(%p), cs(%p), ip(%p), cr2(%p)\n", frame->errno, frame->cs, frame->ip, x86_get_cr2());
            break;

        default:

            // TODO: Handle User Exception

            kpanicf("x86-intr: exception(%p), errno(%p), cs(%p), ip(%p)\n", frame->intno, frame->errno, frame->cs, frame->ip);
            break;

    }


    switch(frame->intno) {

        case 0xFF:
            kpanicf("x86-intr: Spourius Interrupt on cpu #%d\n", 0 /* TODO: SMP */);
            break;

        case 0x20 ... 0xFE:
            kprintf("x86-intr: Unhandled IRQ #%d caught, ignoring\n", frame->intno - 0x20);
            break;

    }

}