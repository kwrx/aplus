#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>

int utime(const char* filename, const struct utimbuf* times) {
	if(!times)
		return errno = EINVAL, -1;
	

	struct timeval tv[2];
	tv[0].tv_sec = times->actime;
	tv[0].tv_usec = 0;
	tv[1].tv_sec = times->modtime;
	tv[1].tv_usec = 0;

	return utimes(filename, tv);
}
