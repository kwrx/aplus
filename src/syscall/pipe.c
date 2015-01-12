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

int sys_pipe(int fd[2]) {
	if(unlikely(!fd)) {
		errno = EINVAL;
		return -1;
	}

	inode_t* nd = (inode_t*) kmalloc(sizeof(inode_t) * 2);
	memset(nd, 0, sizeof(inode_t) * 2);

	if(unlikely(pipe_create(nd) != 0))
		return -1;

	fd[0] = schedule_append_fd(current_task, &nd[0]);
	fd[1] = schedule_append_fd(current_task, &nd[1]);

	return 0;
}

SYSCALL(sys_pipe, 30);
