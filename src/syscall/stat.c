#include <aplus.h>
#include <aplus/syscall.h>
#include <aplus/fs.h>
#include <aplus/task.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>
#include <dirent.h>

extern task_t* current_task;

int sys_stat(const char* filename, struct stat* st) {
	int fd = sys_open(filename, O_RDONLY, 0644);
	if(unlikely(fd < 0))
		return -1;
	
	int ret = sys_fstat(fd, st);
	sys_close(fd);

	return ret;
}

SYSCALL(sys_stat, 13);


