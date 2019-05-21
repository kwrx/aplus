/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
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


#include <aplus.h>
#include <aplus/debug.h>
#include <arch/x86/cpu.h>
#include <stdint.h>

#define BIOS_COM_ADDRESS            0x400


static uint16_t com_address = 0;

void arch_debug_init(void) {

    uint16_t* p = (uint16_t*) (CONFIG_KERNEL_BASE + BIOS_COM_ADDRESS);
    
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

void arch_debug_putc(int ch) {
    if(unlikely(!com_address))
        return;

    int i;
    for(i = 0; i < 100000 && ((inb(com_address + 5) & 0x20) == 0); i++)
        x86_pause();

    outb(com_address, ch);
}