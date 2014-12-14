#include <errno.h>
#undef errno
int errno;

#include "../../libcrt0/src/syscalls.c"
