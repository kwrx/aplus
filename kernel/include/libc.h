/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


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
