
#include <aplus.h>
#include <aplus/syscall.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

#include <sys/time.h>
#include <sys/times.h>

int sys_gettimeofday(struct timeval* tv, struct timezone* tz) {
	if(!tv) {
		errno = EINVAL;
		return -1;
	}
	
	tv->tv_sec = pit_gettime();
	tv->tv_usec = (tv->tv_sec * 1000000) + (pit_getticks() * 1000);
	
	return 0;
}

SYSCALL(sys_gettimeofday, 18);
