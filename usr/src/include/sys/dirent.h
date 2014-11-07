#ifndef _SYS_DIRENT_H
#define _SYS_DIRENT_H

#include <unistd.h>
#include <sys/types.h>

typedef struct __DIR {
	int fd;
	int position;
} DIR;


struct dirent {
	int d_ino;
	char d_name[];
};


#ifndef O_DIRECTORY
#define O_DIRECTORY		00200000
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW		00400000
#endif


#ifdef __cplusplus
extern "C" {
#endif

int alphasort(const struct dirent **a, const struct dirent **b);
int closedir(DIR* d);
DIR* opendir(const char* path);
struct dirent* readdir(DIR* d);
void rewinddir(DIR* d);
int scandir(const char *pathname, struct dirent ***namelist, int (*select)(const struct dirent *), int (*compar)(const struct dirent **, const struct dirent **));
void seekdir(DIR* d, off_t offset);
off_t telldir(DIR* d);


#ifdef __cplusplus
}
#endif

#endif
