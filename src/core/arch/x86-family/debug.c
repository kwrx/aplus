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
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/endian.h>

#include <hal/cpu.h>
#include <hal/vmm.h>
#include <hal/debug.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>


#define BIOS_COM_ADDRESS            0x400


static uint16_t com_address = 0;


/*!
 * @brief Initialize Debugger on UART0.
 * 
 * Read COM Address from SMBios Area and configure Serial Port.
 */
void arch_debug_init(void) {

    uint16_t* p = (uint16_t*) (KERNEL_HIGH_AREA + BIOS_COM_ADDRESS);
    
    for(int i = 0; i < 4; i++) {
        if(p[i] == 0)
            continue;

        com_address = p[i];

        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x80);
        outb(com_address + 0, 0x03);
        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x03);
        outb(com_address + 2, 0xC7);
        outb(com_address + 4, 0x0B);

        break;
    }
}


/*!
 * @brief Write to Debugger.
 * 
 * Wait and write a character on Serial Port.
 */
void arch_debug_putc(char ch) {
    
    if(unlikely(!com_address))
        return;

    int i;
    for(i = 0; i < 100000 && ((inb(com_address + 5) & 0x20) == 0); i++)
        __builtin_ia32_pause();


    if(ch == '\n')
        outb(com_address, '\r');

    outb(com_address, ch);

}




/*!
 * @brief Stacktrace.
 * 
 * ...
 */
void arch_debug_stacktrace(uintptr_t* frames, size_t count) {
    


    struct stack {
        struct stack* bp;
        uintptr_t ip;
    } __packed *frame;


#if defined(__x86_64__)
    __asm__ __volatile__ ("movq %%rbp, %%rax" : "=a"(frame));
#else
    __asm__ __volatile__ ("movl %%ebp, %%rax" : "=a"(frame));
#endif


    int i;
    for(i = 0; frame && i < count; i++) {
        
        frames[i] = 0;

        if(unlikely(!ptr_check(frame, R_OK)))
            break;

        frames[i] = frame->ip;
        frame = frame->bp;

    }
}