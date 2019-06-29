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
#include <aplus/network.h>
#include <aplus/reboot.h>
#include <stdint.h>
#include <dirent.h>


void cmain(void) {

    current_task->priority = TASK_PRIO_MIN;

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
    __init(network, ());


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


    sys_mkdir("/root", 0666);
    sys_mount("/dev/sda1", "/root", "ext2", 0, NULL);


    
    int fd = sys_open("/root", 0, 0);
    if(fd < 0)
        kpanic("fd < 0");

    
    struct dirent d;
    while(sys_getdents(fd, &d, sizeof(struct dirent)) > 0)
        kprintf("<%d> {%d}: %s\n", (int) d.d_ino, (int) d.d_type, d.d_name);


    const char* argv[] = { "/sbin/init", NULL };
    const char* envp[] = { NULL };

    if((e = sys_execve(argv[0], argv, envp)) < 0)
        kpanic("init: execve() %s", strerror(-e));


    arch_reboot(ARCH_REBOOT_HALT);
}