#ifndef _SYS_FCNTL_H_
#define _SYS_FCNTL_H_
#include <sys/_default_fcntl.h>


#undef _FNOFOLLOW
#undef _FDIRECTORY
#define _FNOFOLLOW      0x100000
#define _FDIRECTORY     0x200000


#undef O_NOFOLLOW
#undef O_DIRECTORY
#define O_NOFOLLOW      _FNOFOLLOW
#define O_DIRECTORY     _FDIRECTORY
#endif
