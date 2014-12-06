#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

extern task_t* current_task;


extern int tmpfs_read(inode_t* ino, char* buf, int size);
extern int tmpfs_write(inode_t* ino, char* buf, int size);
extern void tmpfs_flush(inode_t* inode);


inode_t* tmpfs_creat(inode_t* inode, char* name, mode_t mode) {
	if((void*) vfs_mapped(inode, name) != NULL)
		return NULL;
		
	inode_t* ino = (inode_t*) kmalloc(sizeof(inode_t));
	strcpy(ino->name, name);
	
	ino->dev = inode->dev;
	ino->ino = 0;
	ino->mode = mode;
	ino->nlink = 0;
	ino->uid = current_task->uid;
	ino->gid = current_task->gid;
	ino->rdev = ino->rdev;
	ino->size = (size_t) 0;
	ino->atime = ino->ctime = ino->mtime = sys_time(NULL);
	ino->parent = inode;
	ino->link = NULL;
	
	ino->read = tmpfs_write;
	ino->write = tmpfs_read;
	ino->readdir = NULL;
	ino->finddir = NULL;
	ino->creat = NULL;
	ino->rename = NULL;
	ino->unlink = NULL;
	ino->chown = NULL;
	ino->flush = tmpfs_flush;
	ino->ioctl = NULL;
	
	vfs_map(ino);
	
	return ino;
}
