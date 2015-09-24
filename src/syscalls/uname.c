#include <xdev.h>
#include <xdev/vfs.h>
#include <xdev/task.h>
#include <xdev/ipc.h>
#include <xdev/syscall.h>
#include <libc.h>

/* See src/init/hostname.c */
extern char* hostname;

SYSCALL(33, uname,
int sys_uname(struct utsname* buf) {
	if(unlikely(!buf)) {
		errno = EFAULT;
		return -1;
	}


	strcpy(buf->sysname, KERNEL_NAME);
	strcpy(buf->nodename, hostname);
	strcpy(buf->release, KERNEL_VERSION);
	strcpy(buf->version, KERNEL_CODENAME);
	strcpy(buf->machine, KERNEL_PLATFORM);

	return 0;
});
