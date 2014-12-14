#include <aplus.h>
#include <aplus/task.h>
#include <aplus/fs.h>
#include <aplus/fsys.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#include "procfs.h"

extern task_t* current_task;
extern list_t* task_queue;

inode_t* procfs_finddir (inode_t* inode, char* name) {
	if(!inode)
		return NULL;

	int pid = atoi(name);
	if(pid <= 0)
		return NULL;

	inode_t* ino = (inode_t*) kmalloc(sizeof(inode_t));
	memset(ino, 0, sizeof(inode_t));

	strcpy(ino->name, name);
	
	ino->dev = inode->dev;
	ino->ino = PROCFS_INO_START + pid;
	ino->mode = S_IFREG;
	ino->nlink = 0;
	ino->uid = current_task->uid;
	ino->gid = current_task->gid;
	ino->rdev = ino->rdev;
	ino->size = (size_t) 0;
	ino->atime = ino->ctime = ino->mtime = sys_time(NULL);
	ino->parent = inode;
	ino->link = NULL;
	
	ino->read = NULL;
	ino->write = NULL;
	ino->readdir = NULL;
	ino->finddir = NULL;
	ino->creat = NULL;
	ino->rename = NULL;
	ino->unlink = NULL;
	ino->chown = NULL;
	ino->flush = NULL;
	ino->ioctl = NULL;

	return ino;
}
