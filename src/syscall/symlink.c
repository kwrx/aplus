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


int sys_symlink(char* path, char* link) {
	if(unlikely(!current_task))
		return -1;	

	if(unlikely(!path || !link)) {
		errno = EINVAL;
		return -1;
	}

	int dfd = sys_open(link, O_CREAT | O_EXCL | O_RDONLY | O_VIRT, S_IFLNK);
	if(dfd < 0)
		return -1;

	inode_t* dino = current_task->fd[dfd];
	sys_close(dfd);

	int pfd = sys_open(path, O_RDONLY, 0644);
	if(unlikely(pfd < 0))
		return -1;

	inode_t* pino = current_task->fd[pfd];
	sys_close(pfd);

	pino->nlink += 1;
	dino->link = pino;
	
	return 0;	
}

SYSCALL(sys_symlink, 28);
