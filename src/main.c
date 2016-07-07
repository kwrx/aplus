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

	kprintf(INFO, "%s %s-%s %s %s %s\n", KERNEL_NAME, KERNEL_VERSION, KERNEL_CODENAME, KERNEL_DATE, KERNEL_TIME, KERNEL_PLATFORM);


	sys_mount("/dev/cd0", "/cdrom", "iso9660", 0, NULL);
	sys_mount(NULL, "/tmp", "tmpfs", 0, NULL);

	sys_open("/dev/stdin", O_RDONLY, 0);
	sys_open("/dev/stdout", O_WRONLY, 0);
	sys_open("/dev/stderr", O_WRONLY, 0);
	
	
	char* __argv[] = { "/cdrom/usr/bin/test", NULL };
	char* __envp[] = { 0 };


	if(sys_fork() == 0)
		sys_execve(__argv[0], __argv, __envp);
		
	for(;;)
		sys_yield();
}
