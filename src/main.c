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
	

	sys_mount("/dev/hd0", "/hdd", "fat", 0, NULL);
	

	char* buf = "/hdd/LongFileName0";
	int i = 0;
	for(i = 0; i < 64; i++) {
		buf[i / 8 + 5] += 1;
		int fd = sys_open(strdup(buf), O_CREAT | O_RDONLY, S_IFREG | 0666);
		if(fd < 0)kprintf(INFO, "Created %s -> %d (%d)\n", buf, fd, errno);
	}

	for(;;);
}
