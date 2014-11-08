#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/list.h>
#include <aplus/attribute.h>


#include <stdint.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include <errno.h>



extern task_t* current_task;

int sys_mount(const char* dev, const char* dir, const char* fstype, int options, const void* data) {
	if(!current_task)
		return -1;


	if(!im_superuser()) {
		errno = EPERM;
		return -1;
	}


	if(dir == NULL || strlen(dir) == 0) {
		errno = EINVAL;
		return -1;
	}

	inode_t* idev = NULL;
	inode_t* idir = NULL;


	if(dev && strlen(dev) > 0) {
		int dfd = sys_open(dev, O_RDONLY, 0644);
		if(dfd < 0) {
			errno = ENOENT;
			return -1;
		}

		idev = current_task->fd[dfd];
		sys_close(dfd);
	}


	int sfd = sys_open(dir, O_CREAT | O_DIRECTORY | O_RDONLY, S_IFDIR);
	if(sfd < 0) {
		errno = ENOENT;
		return -1;
	}

	idir = current_task->fd[sfd];
	sys_close(sfd);

	list_t* fs = attribute("fs");
	if(list_empty(fs)) {
		errno = ENODEV;
		return -1;
	}
	

	fs_t* found = NULL;

	list_foreach(value, fs) {
		fs_t* f = (fs_t*) value;

		if(strcmp(f->name, fstype) == 0)
			found = f;
	}

	list_destroy(fs);

	if(found == NULL) {
		errno = ENODEV;
		return -1;
	}

	idir->mode |= S_IFMT;
	idir->dev = idev->ino;

	return found->mount(idev, idir, options);
}

