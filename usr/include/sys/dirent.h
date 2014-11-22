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
	char d_name[255];
};


#ifdef O_DIRECTORY
#undef O_DIRECTORY
#endif

#ifdef O_NOFOLLOW
#undef O_NOFOLLOW
#endif

#ifdef O_VIRT
#undef O_VIRT
#endif


#define O_DIRECTORY		00000200000
#define O_NOFOLLOW		00000400000
#define O_VIRT			02000000000


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
