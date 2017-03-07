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
	int p;
    if((p = (int) sysconfig("idle.priority", SYSCONFIG_FORMAT_INT, 0)) > 0) {
        switch(p) {
			case 1:
				kernel_task->priority = TASK_PRIO_MIN;
				break;
			case 2:
				kernel_task->priority = TASK_PRIO_REGULAR;
				break;
			case 3:
				kernel_task->priority = TASK_PRIO_MAX;
				break;
			default:
				for(;;)
					sys_yield();
		}
    }
		
	for(;;)
		__pause__();
}



int main(int argc, char** argv) {
	(void) mm_init();
	(void) syscall_init();
	(void) vfs_init();
#if CONFIG_NETWORK
	(void) network_init();
#endif
	(void) module_init();
	(void) mounts_init();
	
	kprintf(INFO, "%s %s-%s %s %s %s\n", 
			KERNEL_NAME, 
			KERNEL_VERSION,
			KERNEL_CODENAME,
			KERNEL_DATE,
			KERNEL_TIME,
			KERNEL_PLATFORM);

	
	char* __argv[] = { "/usr/sbin/init", NULL };
	char* __envp[] = { NULL };

	if(sys_fork() == 0)
		sys_execve(__argv[0], __argv, __envp);
		

    idle();
	return 0;
}
