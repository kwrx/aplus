#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


extern task_t* current_task;

inode_t* procfs_mknod(char* rpath, mode_t mode) {
	static char buffer[64];
	memset(buffer, 0, sizeof(buffer));

	ksprintf(buffer, "/proc/%s", rpath);


	int fd = sys_open(buffer, O_CREAT | O_EXCL | O_TRUNC, mode | 0644);
	if(fd < 0)
		return NULL;

	inode_t* ino = current_task->fd[fd];
	sys_close(fd);

	return ino;
}
