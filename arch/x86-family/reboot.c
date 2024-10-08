/*
 * Author:
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


#include <stdint.h>
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/hal.h>

#include <arch/x86/acpi.h>
#include <arch/x86/cpu.h>


__noreturn void arch_reboot(int mode) {


    // arch_intr_disable();


    switch (mode) {

        case ARCH_REBOOT_RESTART: {

            /* ACPI */
            // TODO: ACPI Restart

            kprintf("x86-reboot: ACPI reboot failed\n");


            /* Keyboard Reset */

            for (int i = 0; i < 10; i++) {

                for (int j = 0; j < 0x10000; j++) {

                    if ((inb(0x64) & 0x02) == 0)
                        break;

                    __builtin_ia32_pause();
                }


                arch_timer_delay(50);
                outb(0x64, 0xFE);
                arch_timer_delay(50);
            }


            kprintf("x86-reboot: PS/2 reboot failed\n");


            /* Triple Fault */
            uintptr_t invalid[3] = {0};

            __asm__ __volatile__("lidt %0" ::"m"(invalid));
            __asm__ __volatile__("int $3");


            kprintf("x86-reboot: Triple Fault reboot failed\n");

        } break;

        case ARCH_REBOOT_POWEROFF: {

            /* ACPI */
            // TODO: ACPI Power-off

            kprintf("x86-reboot: ACPI power-off failed\n");

            {
                /* QEMU */
                const int QEMU_EXIT_PORT = 0xF4;
                const int QEMU_EXIT_CODE = 0x10;

                outl(QEMU_EXIT_PORT, QEMU_EXIT_CODE);
            }


            {
                /* VirtualBox */
                const int VBOX_PORT  = 0x4004;
                const int VBOX_MAGIC = 0x3400;

                outw(VBOX_PORT, VBOX_MAGIC);
            }

            kprintf("x86-reboot: VM power-off failed\n");

        } break;

        case ARCH_REBOOT_CRASH: {

            {
                /* QEMU */
                const int QEMU_EXIT_PORT = 0xF4;
                const int QEMU_EXIT_CODE = 0x11;

                outl(QEMU_EXIT_PORT, QEMU_EXIT_CODE);
            }

            {
                /* VirtualBox */
                const int VBOX_PORT  = 0x4004;
                const int VBOX_MAGIC = 0x3401;

                outw(VBOX_PORT, VBOX_MAGIC);
            }

        } break;

        case ARCH_REBOOT_SUSPEND:

            kpanicf("arch_reboot(): PANIC! ARCH_REBOOT_SUSPEND not yet supported\n");
            break;

        case ARCH_REBOOT_HALT:
            break;
    }

    for (;;) {
        __cpu_pause();
        __cpu_halt();
    }
}
