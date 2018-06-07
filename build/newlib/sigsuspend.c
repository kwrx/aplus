#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sched.h>
#include <errno.h>

int sigsuspend(const sigset_t* mask) {
	sigwaitinfo(mask, NULL);

	errno = EINTR;
	return -1;
}
