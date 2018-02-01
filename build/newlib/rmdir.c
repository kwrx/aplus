#include <unistd.h>
#include <dirent.h>
#include <errno.h>

int rmdir(const char* path) {
	DIR* d = opendir(path);
	if(!d)
		return -1;

	struct dirent* ent = readdir(d);
	closedir(d);

	if(ent) {
		errno = ENOTEMPTY;
		return -1;
	}

	return unlink(path);
}
