#include "stdio.h"

/* Required syscalls:	*
 * -> unlink			*
 */

int remove(const char* name) {
	if(unlikely(!name)) {
		errno = EINVAL;
		return -1;
	}

	return unlink(name);
}
