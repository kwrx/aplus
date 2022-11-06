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

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/intr.h>



#define BIOS_COM_ADDRESS            0x400

#define COM1_DEFAULT_PORT           0x3F8
#define COM2_DEFAULT_PORT           0x2F8
#define COM3_DEFAULT_PORT           0x3E8
#define COM4_DEFAULT_PORT           0x2E8


static uint16_t com_address = 0;


#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

#define X86_VGA_WIDTH               (80)
#define X86_VGA_HEIGHT              (25)
#define X86_VGA_SIZE                (X86_VGA_WIDTH * X86_VGA_HEIGHT * 2)
#define X86_VGA_ADDRESS             ((uint16_t*) (KERNEL_HIGH_AREA + 0xB8000))

static uint16_t vga_offset = 0;

#endif


/*!
 * @brief Initialize Debugger on UARTx.
 * 
 * Read COM Address from SMBios Area or default ports collection and configure Serial Ports.
 */
void arch_debug_init(void) {

#if defined(CONFIG_X86_HAVE_SMBIOS)
    uint16_t* p = (uint16_t*) (KERNEL_HEAP_AREA + BIOS_COM_ADDRESS);
#else
    uint16_t p[] = { COM1_DEFAULT_PORT, COM2_DEFAULT_PORT, COM3_DEFAULT_PORT, COM4_DEFAULT_PORT };
#endif


    for(int i = 0; i < 4; i++) {

        if(p[i] == 0)
            continue;

        com_address = p[i];

        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x80);
        outb(com_address + 0, 0x01);
        outb(com_address + 1, 0x00);
        outb(com_address + 3, 0x03);
        outb(com_address + 2, 0xC7);
        outb(com_address + 4, 0x0B);

        break;
    }


#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

    vga_offset  = 0;

    memset((void*) (X86_VGA_ADDRESS), 0x00, X86_VGA_SIZE);
    
#endif

}


/*!
 * @brief Write to Debugger.
 * 
 * Wait and write a character on Serial Port.
 */
void arch_debug_putc(char ch) {
    
    if(likely(com_address)) {

        int i;
        for(i = 0; i < 100000 && ((inb(com_address + 5) & 0x20) == 0); i++)
            __builtin_ia32_pause();


        if(unlikely(ch == '\n'))
            outb(com_address, '\r');

        outb(com_address, ch);


        for(i = 0; i < 100000 && ((inb(com_address + 5) & 0x20) == 0); i++)
            __builtin_ia32_pause();

    }


#if defined(CONFIG_X86_ENABLE_DEBUG_VGA)

    if(unlikely(vga_offset >= X86_VGA_WIDTH * X86_VGA_HEIGHT)) {

        memmove (
            &(X86_VGA_ADDRESS)[0], 
            &(X86_VGA_ADDRESS)[X86_VGA_WIDTH], 
            X86_VGA_SIZE - (X86_VGA_WIDTH * 2)
        );

        memset (
            &(X86_VGA_ADDRESS)[X86_VGA_WIDTH * (X86_VGA_HEIGHT - 1)],
            0x00, 
            X86_VGA_WIDTH * 2
        );

        vga_offset -= X86_VGA_WIDTH;

    }


    switch(ch) {
        case '\r':
            vga_offset -= vga_offset % X86_VGA_WIDTH;
            break;
        case '\n':
            vga_offset += X86_VGA_WIDTH - (vga_offset % X86_VGA_WIDTH);
            break;
        case '\v':
            vga_offset += X86_VGA_WIDTH;
            break;
        case '\b':
            vga_offset -= 1;
            break;
        case '\t':
            vga_offset += 8 - (vga_offset % 8);
            break;
        default:
            X86_VGA_ADDRESS[vga_offset++] = (0x07 << 8) | ch;
            break;
    }
    
#endif

}



/*!
 * @brief Stacktrace.
 * 
 * Print stacktrace on Serial Port.
 */
void arch_debug_stacktrace(uintptr_t* frames, size_t count) {
    

    if(!current_task)
        return;
        

    struct stack {
        struct stack* bp;
        uintptr_t ip;
    } __packed *frame;


#if defined(__x86_64__)
    __asm__ __volatile__ ("movq %%rbp, %%rax" : "=a"(frame));
#elif defined(__i386__)
    __asm__ __volatile__ ("movl %%ebp, %%rax" : "=a"(frame));
#else
#error "Unsupported Architecture"
#endif


    int i;
    for(i = 0; frame && i < count; i++) {
        
        frames[i] = 0;

        if(unlikely(!uio_check(frame, R_OK)))
            break;

        if(unlikely(!uio_check(frame, S_OK))) {

#if defined(__x86_64__)
            frames[i] = uio_r64((uintptr_t) frame + offsetof(struct stack, ip));
            frame     = (struct stack*) uio_r64((uintptr_t) frame + offsetof(struct stack, bp));
#else
            frames[i] = uio_r32((uintptr_t) frame + offsetof(struct stack, ip));
            frame     = (struct stack*) uio_r32((uintptr_t) frame + offsetof(struct stack, bp));
#endif

        } else {

            frames[i] = frame->ip;
            frame     = frame->bp;

        }    

    }

}