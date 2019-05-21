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
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/mm.h>
#include <aplus/syscall.h>
#include <stdint.h>


void cmain(void) {
    for(;;);
}

void kmain(void) {

    core_init();
    pmm_init();
    arch_init();

    syscall_init();
    vfs_init();


    kprintf("cpu: %s %d-%d MHz (Cores: %d, Threads: %d)\n",
        mbd->cpu.family,
        mbd->cpu.min_speed,
        mbd->cpu.max_speed,
        mbd->cpu.max_cores,
        mbd->cpu.max_threads);
    
    kprintf("boot: %s %s\n",
        KERNEL_NAME,
        mbd->cmdline);
    
    kprintf("%s %s %s %s %s (%p:%lluMB)\n", 
        KERNEL_NAME, 
        KERNEL_VERSION,
        KERNEL_DATE,
        KERNEL_TIME,
        KERNEL_PLATFORM,
        mbd->memory.start, 
        mbd->memory.size / 1024 / 1024);

}