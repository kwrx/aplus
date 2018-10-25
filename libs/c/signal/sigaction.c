#include <signal.h>
#include <errno.h>
#include <string.h>


int sigaction(int sig, const struct sigaction *restrict sa, struct sigaction *restrict old) {
	if (sig - 32U < 3 || sig - 1U >= _NSIG - 1) {
		errno = EINVAL;
		return -1;
	}
	
	return rt_sigaction(sig, sa, old, sizeof(sa->sa_mask));
}
