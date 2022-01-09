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
                                                                        
#ifndef _APLUS_X86_INTR_H
#define _APLUS_X86_INTR_H

#ifndef __ASSEMBLY__

#include <aplus.h>
#include <aplus/debug.h>


//? NOTE: IRQs allocatable from drivers (see MSI-X)
//? @see 10.11.2 - Message Data Register Format
#define IRQ_MSIX_ALLOCATABLE_OFFSET             (0x10)  // 16 or higher


typedef struct {

    uintptr_t di;
    uintptr_t si;
    uintptr_t bp;
    uintptr_t bx;
    uintptr_t dx;
    uintptr_t cx;
    uintptr_t ax;

#if defined(__x86_64__)
    uintptr_t r8;
    uintptr_t r9;
    uintptr_t r10;
    uintptr_t r11;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
#endif

    uintptr_t intno;
    uintptr_t errno;

    uintptr_t ip;
    uintptr_t cs;
    uintptr_t flags;

    uintptr_t sp;
    uintptr_t ss;
    
} __packed interrupt_frame_t;


typedef struct {

    union {
        
        struct {
            
            void* ustack;
            void* kstack;

            long flags;
            sigset_t mask;
            interrupt_frame_t regs;

        };

        char __padding[512 - 16];
    };

    char fpuregs[0];

} __packed sigcontext_frame_t;



__BEGIN_DECLS

void timer_init();

__END_DECLS

#endif
#endif
