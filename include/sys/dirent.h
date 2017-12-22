#ifndef _SYS_DIRENT_H
# define _SYS_DIRENT_H

#include <unistd.h>
#include <sys/stat.h>

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


#define DT_BLK			(S_IFBLK >> 12)
#define DT_CHR			(S_IFCHR >> 12)
#define DT_DIR			(S_IFDIR >> 12)
#define DT_FIFO			(S_IFIFO >> 12)
#define DT_LNK			(S_IFLNK >> 12)
#define DT_REG			(S_IFREG >> 12)
#define DT_SOCK			(S_IFSCK >> 12)
#define DT_UNKNOWN		(0xFF)

#include <sys/types.h>
#include <bits/dirent.h>

#endif
