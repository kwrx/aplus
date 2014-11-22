#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

extern struct dirent* procfs_readdir (struct inode* inode, int index);
extern struct inode* procfs_finddir (struct inode* inode, char* name);
extern struct inode* procfs_creat (struct inode* inode, char* name, mode_t mode);
extern int procfs_unlink (struct inode* inode, char* name);


int procfs_mount(inode_t* dev, inode_t* ino, int flags) {
	if(!ino)
		return -1;


	ino->finddir = procfs_finddir;
	ino->readdir = procfs_readdir;
	ino->creat = procfs_creat;
	ino->unlink = procfs_unlink;

	return 0;
}

FSYS(procfs, procfs_mount);
