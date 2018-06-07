#include <signal.h>
#include <errno.h>

int sigaction(int sig, const struct sigaction* restrict act, struct sigaction* oact) {
	if(!act) {
		errno = EINVAL;
		return -1;
	}

	_sig_func_ptr h = signal(sig, act->sa_handler);

	if(oact)
		oact->sa_handler = h;

	return 0;
}
