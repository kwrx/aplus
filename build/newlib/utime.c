#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>

int utime(const char* filename, const struct utimbuf* times) {
	return 0;
}

int utimes(const char* filename, const struct timeval times[2]) {
    return 0;
}


int gettimeofday(struct timeval* tv, struct timeval* tz) {
	(void*) tz;

	if(!tv) {
		errno = EINVAL;
		return -1;
	}

	struct timespec ts;
	if(clock_gettime(CLOCK_REALTIME, &ts) != 0) {
		errno = EINVAL;
		return -1;
	}

	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;

	return 0;
}
