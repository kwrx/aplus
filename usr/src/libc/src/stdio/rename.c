#include "stdio.h"

/* Required syscalls:	*
 * -> link				*
 */

int rename(const char* oldname, const char* newname) {
	if(unlikely(!oldname || !newname)) {
		errno = EINVAL;
		return -1;
	}

	if(unlikely(link(oldname, newname) != 0))
		return -1;

	if(unlikely(remove(oldname) != 0))
		return -1;

	return 0;
}
