#include "stdlib.h"

extern void (*__libc_at_quick_exit_handler) (void);

void quick_exit(int status) {
	if(__libc_at_quick_exit_handler)
		__libc_at_quick_exit_handler();

	_exit(status);
}
