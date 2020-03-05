#ifndef	_APLUS_ERRNO_H
#define	_APLUS_ERRNO_H

#ifndef __ASSEMBLY__
#include <errno.h>

#if defined(KERNEL)
#ifdef errno
#undef errno
#endif
#define errno current_cpu->errno
#endif

#endif
#endif