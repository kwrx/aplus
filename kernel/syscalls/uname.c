#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/ipc.h>
#include <aplus/syscall.h>
#include <aplus/debug.h>
#include <libc.h>

/* See src/init/hostname.c */
extern char* hostname;

SYSCALL(33, uname,
int sys_uname(struct utsname* buf) {
	if(unlikely(!buf)) {
		errno = EFAULT;
		return -1;
	}
	
	
	char* __hostname = hostname;


	int fd = sys_open("/etc/hostname", O_RDONLY, 0666);
	if(likely(fd >= 0)) {
		static char buf[BUFSIZ];
		memset(buf, 0, BUFSIZ);
		
		read(fd, buf, BUFSIZ);
		close(fd);
		
		__hostname = buf;
	}

	strcpy(buf->sysname, KERNEL_NAME);
	strcpy(buf->nodename, __hostname);
	strcpy(buf->release, KERNEL_VERSION);
	strcpy(buf->version, KERNEL_CODENAME);
	strcpy(buf->machine, KERNEL_PLATFORM);

	return 0;
});
