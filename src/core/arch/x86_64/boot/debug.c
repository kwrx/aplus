#include <stdint.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>
#include <aplus/core/endian.h>

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

    outb(com_address, ch);
}