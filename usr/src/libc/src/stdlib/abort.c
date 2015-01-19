#include "stdlib.h"

void abort(void) {
#if HAVE_SIGNALS
	raise(SIGABRT);
#else
	_exit(SIGABRT);
#endif
	
	for(;;);
}
