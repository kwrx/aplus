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
#include <aplus/intr.h>
#include <aplus/reboot.h>
#include <arch/x86/cpu.h>
#include <arch/x86/acpi.h>
#include <stdint.h>
#include <string.h>




void arch_reboot(int mode) {


    arch_intr_disable();


    switch (mode) {

        case ARCH_REBOOT_RESTART: {
            
            /* ACPI */


            DEBUG_WARNING(0 && "ACPI Reboot Failed");


            /* Keyboard Reset */

            int i;
            for(i = 0; i < 10; i++) {

                int j;
                for(j = 0; j < 0x10000; j++) {

                    if((inb(0x64) & 0x02) == 0)
                        break;

                    __builtin_ia32_pause();
                }


                arch_timer_delay(50);
                outb(0x64, 0xFE);
                arch_timer_delay(50);
            }


            DEBUG_WARNING(0 && "Keyboard Reboot Failed");


            /* Triple Fault */
            uintptr_t invalid[3] = { 0 };

            __asm__ __volatile__ ("lidt %0" :: "m"(invalid));
            __asm__ __volatile__ ("int 3");


            DEBUG_WARNING(0 && "Triple Fault Reboot Failed");

        }
            break;


        case ARCH_REBOOT_SUSPEND:
            DEBUG_WARNING(0 && "ARCH_REBOOT_SUSPEND not yet supported");
            break;

        case ARCH_REBOOT_POWEROFF: {

            /* ACPI */
        

            DEBUG_WARNING(0 && "ACPI Poweroff Failed");


            /* Virtual Machine */
            outw(0xB004, 0x2000);   /* Bochs */
            outw(0x0604, 0x2000);   /* Qemu  */
            outw(0x4004, 0x3400);   /* Vbox  */

            DEBUG_WARNING(0 && "VM Poweroff Failed");

        }
            break;

        case ARCH_REBOOT_HALT:
            break;
    }


    kpanic("System Halted!");
}