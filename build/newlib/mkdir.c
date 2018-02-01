#include <unistd.h>
#include <fcntl.h>


int mkdir(const char* path, mode_t mode) {
	int fd = open(path, O_CREAT | O_EXCL | O_RDONLY, S_IFDIR | mode);
	if(fd < 0)
		return -1;

	close(fd);
	return 0;
}
