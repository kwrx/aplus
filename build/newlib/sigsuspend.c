#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/sched.h>
#include <errno.h>

static _sig_func_ptr __handlers[NSIG] = { 0 };
static int __raised = 0;
static void __handler(int sig) {
	__raised = sig;
}

int sigsuspend(const sigset_t* mask) {
	__raised = 0;
	memset(__handlers, 0, sizeof(_sig_func_ptr) * NSIG);

	int i;
	for(i = 0; i < NSIG; i++)
		if(*mask & (1 << i))
			__handlers[i] = signal(i, __handler);


	sigset_t oldmask;
	if(sigprocmask(SIG_SETMASK, mask, &oldmask) != 0)
		return -1;

	while(!__raised)
		sched_yield();

	for(i = 0; i < NSIG; i++)
		if(__handlers[i])
			signal(i, __handlers[i]);

	sigprocmask(SIG_SETMASK, &oldmask, NULL);
	if(raise(__raised) != 0)
		return -1;

	errno = EINTR;
	return -1;
}
