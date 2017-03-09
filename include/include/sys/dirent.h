#ifndef _SYS_DIRENT_H
# define _SYS_DIRENT_H

/*
 * This file was written to be compatible with the BSD directory
 * routines, so it looks like it.  But it was written from scratch.
 * Sean Eric Fagan, sef@Kithrup.COM
 */

typedef struct _dirdesc {
	int	dd_fd;
	long	dd_loc;
	long	dd_size;
	char	*dd_buf;
	int	dd_len;
	long	dd_seek;
} DIR;

# define __dirfd(dp)	((dp)->dd_fd)

DIR *opendir (const char *);
struct dirent *readdir (DIR *);
int readdir_r (DIR *__restrict, struct dirent *__restrict,
               struct dirent **__restrict);
void rewinddir (DIR *);
int closedir (DIR *);

#include <sys/types.h>
#include <bits/dirent.h>

#endif
