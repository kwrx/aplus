#include <time.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/times.h>

#include <errno.h>

int clock_gettime(clockid_t id, struct timespec* tp) {
	if(!tp) {
		errno = EINVAL;
		return -1;
	}

	tp->tv_nsec = (int)(((double) clock() / CLOCKS_PER_SEC) * 1000000000);
	tp->tv_sec = (int)((double) clock() / CLOCKS_PER_SEC);

	return 0;
}
