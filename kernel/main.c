#include <aplus.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/vfs.h>
#include <aplus/module.h>
#include <aplus/network.h>
#include <aplus/sysconfig.h>



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

    do {
        pid_t p = sys_fork();
        switch(p) {
            case -1:
                kprintf(ERROR "PANIC! fork() failed! System Halted!\n");
                return 0;
            
            case 0:
                if(sys_execve(__argv[0], __argv, __envp) < 0)
                    kprintf(ERROR "%s: %s\n", __argv[0], strerror(errno));

                sys_exit(-1);

            default:
                current_task->name = "[systemd]";
                current_task->description = "System Process";
                break;
        }
        

        if(sys_waitpid(p, NULL, 0) < 0) {
            kprintf(WARN "waitpid(): could not wait for init process!\n");
            
            for(;;)
                sys_pause();
        }
    } while(1);

    return 0;
}