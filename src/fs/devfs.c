#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <dirent.h>
#include <errno.h>

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/mm.h>


extern inode_t* vfs_root;
extern task_t* current_task;


inode_t* devfs = NULL; 



struct inode* devfs_creat (struct inode* inode, char* name, mode_t mode) {
	if((void*) vfs_mapped(inode, name) != NULL)
		return NULL;
		
	inode_t* ino = (inode_t*) kmalloc(sizeof(inode_t));
	memset(ino, 0, sizeof(inode_t));

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
	
	vfs_map(ino);
	
	return ino;
}


inode_t* devfs_mount() {
	
	devfs = (inode_t*) kmalloc(sizeof(inode_t));
	memset(devfs, 0, sizeof(inode_t));

	strcpy(devfs->name, "dev");
	
	devfs->dev = (dev_t) 0;
	devfs->ino = (ino_t) 0;
	devfs->mode = S_IFDIR;
	devfs->nlink = 0;
	devfs->uid = UID_ROOT;
	devfs->gid = GID_ROOT;
	devfs->rdev = (dev_t) 0;
	devfs->size = (size_t) 0;
	devfs->atime = devfs->ctime = devfs->mtime = sys_time(NULL);
	devfs->parent = vfs_root;
	devfs->link = NULL;
	
	devfs->read = NULL;
	devfs->write = NULL;
	devfs->readdir = NULL;
	devfs->finddir = NULL;
	devfs->creat = devfs_creat;
	devfs->rename = NULL;
	devfs->unlink = NULL;
	devfs->chown = NULL;
	devfs->flush = NULL;
	devfs->ioctl = NULL;
	
	return devfs;
}







inode_t* devfs_getdevice(dev_t dev) {
	int index = 0;
	inode_t* map = NULL;

	while((map = (inode_t*) vfs_mapped_at_index(devfs, index++)) != NULL)
		if(map->ino == dev)
			return map;
	
	return NULL;
}

inode_t* devfs_makedevice(char* name, mode_t mode) {
#ifdef DEV_DEBUG
	kprintf("devfs: installed device %s (%x) {\n", name, mode);
#endif	
	return devfs_creat(devfs, name, mode);
}




EXPORT_SYMBOL(devfs_getdevice);
EXPORT_SYMBOL(devfs_makedevice);
