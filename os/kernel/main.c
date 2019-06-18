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
#include <aplus/smp.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/module.h>
#include <stdint.h>
#include <dirent.h>


void cmain(void) {
    for(;;);
}


void kmain(void) {

    core_init();

    __init(mm,      (MM_INIT_PMM));
    __init(arch,    ());
    __init(mm,      (MM_INIT_VMM | MM_INIT_SLAB));
    __init(task,    ());
    __init(syscall, ());
    __init(vfs,     ());
    __init(sched,   ());


    int e;
    if((e = sys_mount(NULL, "/", "tmpfs", 0, NULL)) < 0)
        kpanic("mount: could not mount fake root: %s", strerror(-e));

    __init(module,  ());


#if defined(DEBUG)
    //__init(unittest, ());
#endif

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


    

    int fd = sys_open("/dev/sda", 2, 0);
    if(fd < 0)
        kpanic("error: %s", strerror(-fd));

    ktime_t tm = arch_timer_gettime();

    char bf[512] = { 0 };
    int r = sys_read(fd, bf, 512);

    DEBUG_ASSERT(r > 0);

    int i;
    for(i = 0; i < 512; i++)
        kprintf("%x ", bf[i] & 0xFF);

    char* buf = kmalloc(64 * 1024, GFP_KERNEL);
    int bs;
    kprintf("%p\n", buf);
    while((e = sys_read(fd, buf,  16 * 1024)) > 0) {
        bs += e;

        if(arch_timer_gettime() != tm) {
            kprintf("sda: reading %d KB/s (%d)\n", bs / 1024, current_task->fd[fd].position >> 9);
        
            tm = arch_timer_gettime();
            bs = 0;
        }
    }

    sys_close(fd);
    kprintf("DONE!\n");

    cmain();
}