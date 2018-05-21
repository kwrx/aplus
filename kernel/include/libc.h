#define _POSIX_THREAD_CPUTIME
#define _POSIX_CPUTIME
#define _POSIX_MONOTONIC_CLOCK

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <sys/termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/string.h>
#include <sys/poll.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include <sched.h>
#include <signal.h>
#include <libgen.h>

#ifdef _POSIX_SOURCE
#define _OLD_POSIX_SOURCE _POSIX_SOURCE
#undef _POSIX_SOURCE
#include <sys/wait.h>
#define _POSIX_SOURCE _OLD_POSIX_SOURCE
#undef _OLD_POSIX_SOURCE
#endif
