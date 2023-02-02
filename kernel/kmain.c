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
                                                                        
#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/syscall.h>
#include <aplus/module.h>
#include <aplus/network.h>
#include <aplus/hal.h>


void cmain(void) {

    current_task->priority = TASK_PRIO_MIN;

    for(;;) {
        __cpu_pause();
        __cpu_halt();
    }

}


void kmain(void) {

    // Initialize syscall
    extern void syscall_init(void);
    syscall_init();

    // Initialize VFS
    extern void vfs_init(void);
    vfs_init();

#if defined(CONFIG_HAVE_NETWORK)
    // Initialize network
    extern void network_init(void);
    network_init();
#endif


    // Mount fake root
    {

        int e = sys_mount(NULL, "/", "tmpfs", 0, NULL);
        if (e < 0) {
            kpanicf("mount: could not mount fake root: errno(%s)", strerror(-e));
        }

    }

    // Initialize module
    extern void module_init(void);
    module_init();

    // Initialize root
    extern void root_init(void);
    root_init();

#if defined(CONFIG_HAVE_SMP)
    // Initialize SMP
    extern void smp_init(void);
    smp_init();
#endif

#if defined(CONFIG_HAVE_TEST)
    // Initialize test
    extern void test_init(void);
    test_init();
#endif


    kprintf ("core: %s %s-%s (%s)\n", CONFIG_SYSTEM_NAME,
                                      CONFIG_SYSTEM_VERSION,
                                      CONFIG_SYSTEM_CODENAME,
                                      CONFIG_COMPILER_HOST);
        
    kprintf("core: built with gcc %s (%s)\n", __VERSION__,
                                              __TIMESTAMP__);

    kprintf("core: boot completed in %d ms, %d KiB of memory used\n", arch_timer_generic_getms(), pmm_get_used_memory() >> 10);



    // Execute init
    {

        const char* __argv[2] = { "/sbin/init", NULL };
        const char* __envp[1] = { NULL };

        int e = sys_execve(__argv[0], __argv, __envp);
        if(e < 0) {
            kpanicf("init: PANIC! execve(%s) failed with errno(%s)\n", __argv[0], strerror(-e));
        }

    }

    arch_reboot(ARCH_REBOOT_HALT);

}
