#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/list.h>
#include <aplus/attribute.h>
#include <aplus/syscall.h>


#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include <errno.h>

#include <sys/utsname.h>

int sys_uname(struct utsname* u) {
	if(!u) {
		errno = EINVAL;
		return -1;
	}

	memset(u, 0, sizeof(struct utsname));

	ksprintf(u->sysname, "%s", KERNEL_NAME);
	ksprintf(u->nodename, "%s", KERNEL_HOSTNAME);

	ksprintf(
		u->release,
		KERNEL_RELEASE_FORMAT,
		KERNEL_RELEASE_MAJOR,
		KERNEL_RELEASE_MINOR,
		KERNEL_RELEASE_LOWER,
		KERNEL_RELEASE_SUFFIX
	);

	ksprintf(
		u->version,
		KERNEL_VERSION_FORMAT,
		KERNEL_VERSION_CODENAME,
		KERNEL_VERSION_DATE,
		KERNEL_VERSION_TIME
	);

	ksprintf(u->machine, "%s", KERNEL_MACHINE);
	return 0;	
}


SYSCALL(sys_uname, 33);
