
#include <stdint.h>
#include <pthread.h>

#include "pthread_internal.h"

PUBLIC int pthread_cancel(pthread_t thread) {
	return pthread_detach(thread);
}


PUBLIC int pthread_setcancelstate(int state, int *oldstate) {
	errno = ENOSYS;
	return -1;
}


PUBLIC int pthread_setcanceltype(int type, int *oldtype) {
	errno = ENOSYS;
	return -1;
}

PUBLIC void pthread_testcancel(void) {
	/* Boh ?? */
}
