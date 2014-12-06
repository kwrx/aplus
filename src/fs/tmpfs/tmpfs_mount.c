#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>


extern struct inode* tmpfs_creat (struct inode* inode, char* name, mode_t mode);
extern int tmpfs_unlink (struct inode* inode, char* name);


int tmpfs_mount(inode_t* dev, inode_t* ino, int flags) {
	if(!ino)
		return -1;


	ino->finddir = 0;
	ino->readdir = 0;
	ino->creat = tmpfs_creat;
	ino->unlink = tmpfs_unlink;

	return 0;
}

FSYS(tmpfs, tmpfs_mount);
