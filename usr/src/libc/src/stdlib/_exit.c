#include "stdlib.h"

/* Required syscalls:	*
 * -> sys_exit
 */

extern __attribute__((noreturn)) void sys_exit(int);

__attribute__((noreturn))
void _exit(int status) {
	sys_exit(status);
}
