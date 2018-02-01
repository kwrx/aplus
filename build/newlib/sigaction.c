#include <signal.h>
#include <errno.h>

int sigaction(int sig, const struct sigaction* restrict act, struct sigaction* oact) {
	errno = ENOSYS;
	return -1;
}
