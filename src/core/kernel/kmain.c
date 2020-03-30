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
                                                                        
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/module.h>
#include <aplus/network.h>
#include <aplus/hal.h>




#define __init(fn, p)           \
    extern void fn##_init();    \
    fn##_init p 




void cmain(void* arg) {
    (void) arg;

    current_task->priority = TASK_PRIO_MIN;

    for(;;) {

        // TODO: ...

#if defined(__i386__) || defined(__x86_64__)
        __builtin_ia32_pause();
#endif
    }

}

void ccmain(void* arg) {
    uint64_t t0 = arch_timer_percpu_getms() + 1000;
    for(;; sched_yield()) {

        if(arch_timer_percpu_getms() < t0)
            continue;

        if(current_task->tid == 4) {
            kprintf("TID 4 CHANGE CPU FROM %d (sp0: %p)\n", ~current_task->affinity, current_task->sp0);

            dump_frame();

            if(current_task->affinity == ~(1 << 0))
                current_task->affinity = ~(1 << 1);
            else
                current_task->affinity = ~(1 << 0);
            
        }

        //kprintf("Task %s %d lived! (memory: %d KiB, timer: %d)\n", current_task->argv[0], current_task->tid, pmm_get_used_memory() >> 10, arch_timer_percpu_getms());
        t0 = arch_timer_percpu_getms() + 1000;
    }
}




void kmain() {

#if defined(CONFIG_HAVE_SMP)
    __init(smp,     ());
#endif

    __init(syscall, ());
    __init(vfs,     ());

#if defined(CONFIG_HAVE_NETWORK)
    //__init(network, ());  // TODO: Rewrite network/sys.c
#endif



    int e;
    if((e = sys_mount(NULL, "/", "tmpfs", 0, NULL)) < 0)
        kpanicf("mount: could not mount fake root: errno(%s)", strerror(-e));

    __init(module,  ());
    __init(root,    ());


    arch_task_spawn_kthread("[bsp]", ccmain, 51200, NULL);
    arch_task_spawn_kthread("[bsp2]", ccmain, 51002, NULL);
    arch_task_spawn_kthread("[bsp3]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp4]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp5]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp6]", ccmain, 51200, NULL);
    arch_task_spawn_kthread("[bsp7]", ccmain, 51002, NULL);
    arch_task_spawn_kthread("[bsp8]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp9]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp10]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp11]", ccmain, 51200, NULL);
    arch_task_spawn_kthread("[bsp12]", ccmain, 51002, NULL);
    arch_task_spawn_kthread("[bsp13]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp14]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp15]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp16]", ccmain, 51200, NULL);
    arch_task_spawn_kthread("[bsp17]", ccmain, 51002, NULL);
    arch_task_spawn_kthread("[bsp18]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp19]", ccmain, 51020, NULL);
    arch_task_spawn_kthread("[bsp20]", ccmain, 51020, NULL);


    kprintf ("core: %s %s-%s (%s)\n", CONFIG_SYSTEM_NAME,
                                      CONFIG_SYSTEM_VERSION,
                                      CONFIG_SYSTEM_CODENAME,
                                      CONFIG_COMPILER_HOST);
        
    kprintf("core: built with gcc %s (%s)\n", __VERSION__,
                                              __TIMESTAMP__);

    kprintf("core: boot completed in %d ms, %d KiB of memory used\n", 
            arch_timer_percpu_getms(), 
            pmm_get_used_memory() >> 10
    );



    const char* __argv[2] = { "/sbin/init", NULL };
    const char* __envp[1] = { NULL };

    if((e = sys_execve(__argv[0], __argv, __envp)) < 0)
        kpanicf("init: PANIC! execve() failed with errno(%s)\n", strerror(-e));


    arch_reboot(ARCH_REBOOT_HALT);

}
