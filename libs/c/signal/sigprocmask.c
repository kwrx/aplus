#include <signal.h>
#include <errno.h>
#include <string.h>


int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
	return rt_sigprocmask(how, set, oldset, _NSIG / sizeof(long));
}
