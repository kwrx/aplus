#include <xdev.h>
#include <xdev/mm.h>
#include <xdev/debug.h>
#include <xdev/syscall.h>
#include <xdev/task.h>
#include <xdev/vfs.h>
#include <xdev/module.h>
#include <xdev/network.h>


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


	
	char* __argv[] = { "/usr/bin/init", "--console", NULL };
	char* __envp[] = { NULL };

	if(sys_fork() == 0)
		sys_execve(__argv[0], __argv, __envp);
		
		
	for(;;)
		sys_yield();
}
