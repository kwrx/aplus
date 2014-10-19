
#include <aplus.h>
#include <aplus/syscall.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>


pid_t sys_getpid() {
	return schedule_getpid();
}


SYSCALL(sys_getpid, 5);
