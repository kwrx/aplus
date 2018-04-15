#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <aplus/module.h>
#include <aplus/network.h>
#include <aplus/sysconfig.h>


static void idle() {
    current_task->name = "[systemd]";
    current_task->description = "System Idle Process";

    int p;
    if((p = (int) sysconfig("idle.priority", 0)) > 0) {
        switch(p) {
            case 1:
                current_task->priority = TASK_PRIO_MIN;
                break;
            case 2:
                current_task->priority = TASK_PRIO_REGULAR;
                break;
            case 3:
                current_task->priority = TASK_PRIO_MAX;
                break;
        }
    }
        
    for(;;)
        __pause__();
}



int main(int argc, char** argv) {
    (void) libk_init();
    (void) mm_init();
    (void) syscall_init();
    (void) vfs_init();
#if CONFIG_NETWORK
    (void) network_init();
#endif
    (void) module_init();
    (void) mounts_init();
#if CONFIG_IOSCHED
    (void) iosched_init();
#endif
    (void) local_init();


    kprintf(INFO "cpu: %s %d MHz (Cores: %d, Threads: %d)\n",
            mbd->cpu.family,
            mbd->cpu.speed,
            mbd->cpu.cores,
            mbd->cpu.threads);
    
    kprintf(INFO "boot: %s %s\n",
            KERNEL_NAME,
            mbd->cmdline.args);
    
    kprintf(INFO "%s %s-%s %s %s %s (%p:%dMB)\n", 
            KERNEL_NAME, 
            KERNEL_VERSION,
            KERNEL_CODENAME,
            KERNEL_DATE,
            KERNEL_TIME,
            KERNEL_PLATFORM,
            mbd->memory.start, 
            (int) mbd->memory.size / 1024 / 1024);
        

    
    char* __argv[] = { "/usr/sbin/init", NULL };
    char* __envp[] = { NULL };

    if(sys_fork() == 0) {
        if(sys_execve(__argv[0], __argv, __envp) < 0)
            kprintf(ERROR "%s: %s\n", __argv[0], strerror(errno));

        sys_exit(-1);
    }

    idle();
    return 0;
}