/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <arch/x86_64/x86_64.h>

extern int intr_init();
extern int timer_init();
extern int task_init();
extern int pci_init();



void x86_64_init() {
    __builtin_cpu_init();

    uintptr_t p;
    for(p = 0x000F0000; p < 0x000FFFFF; p += 16) {
        if(strncmp((char*) p, "_SM_", 4) != 0)
            continue;

        uintptr_t sta = *(uintptr_t*) (p + 0x18);
        uint16_t len = *(uint16_t*) (p + 0x1C);

        int i;
        for(i = 0; i < len; i++) {
            if(*(uint8_t*) (sta) != 4) {
                sta += *(uint8_t*) (sta + 0x01);

                while(*(uint16_t*) sta != 0)
                    sta++;

                sta += 2;
                continue;
            }

            mbd->cpu.family = "Unknown"; //x86_cpu_family[*(uint8_t*) (sta + 0x06)];
            mbd->cpu.speed = *(uint16_t*) (sta + 0x16);
            mbd->cpu.cores = *(uint8_t*) (sta + 0x23);
            mbd->cpu.threads = *(uint8_t*) (sta + 0x24);

            break;
        }
    }


    (void) intr_init();
    (void) pci_init();
    (void) timer_init();
    (void) task_init();
}
