
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
	
	tv->tv_sec = timer_gettime();
	tv->tv_usec = timer_getticks() * (1000000 / timer_getfreq());
	
	return 0;
}

SYSCALL(sys_gettimeofday, 18);
