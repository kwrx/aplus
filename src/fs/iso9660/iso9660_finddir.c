#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>
#include <aplus/mm.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"


extern int iso9660_read(inode_t*, char*, int);
extern struct dirent* iso9660_readdir(inode_t*, int);


inode_t* iso9660_finddir(inode_t* ino, char* name) {
	if(!ino)
		return NULL;
		
	if(!ino->dev)
		return NULL;

	if(!ino->userdata)
		return NULL;

	if(!name || strlen(name) == 0)
		return NULL;


	inode_t* dev = (inode_t*) devfs_getdevice(ino->dev);
	if(!dev)
		return NULL;

	iso9660_dir_t* dir = (iso9660_dir_t*) ino->userdata;
	iso9660_dir_t* nodes = (iso9660_dir_t*) kmalloc(iso9660_getlsb32(dir->length));
	iso9660_dir_t* snodes = nodes;


	dev->position = iso9660_getlsb32(dir->lba) * ISO9660_SECTOR_SIZE;
	if(fs_read(dev, nodes, iso9660_getlsb32(dir->length)) != iso9660_getlsb32(dir->length)) {
		kfree(nodes);
		return NULL;
	}

	for(;;) {
		if(nodes->size == 0) {
			kfree(snodes);
			return 0;
		}

		char* nodename = (char*) kmalloc(ISO9660_NAME_LENGTH);
		memset(nodename, 0, nodes->idlen);

		strncpy(nodename, nodes->reserved, nodes->idlen);
		iso9660_checkname(nodename);


		if(strcmp(nodename, name) == 0)
			break;

		kfree(nodename);
		nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	}


	inode_t* f = (inode_t*) kmalloc(sizeof(inode_t));
	memset(f, 0, sizeof(inode_t));

	strcpy(f->name, name);
	
	f->dev = ino->dev;
	f->ino = 0;
	f->nlink = 0;
	f->uid = ino->uid;
	f->gid = ino->gid;
	f->rdev = ino->rdev;
	f->size = (size_t) iso9660_getlsb32(nodes->length);
	f->atime = f->ctime = f->mtime = sys_time(NULL);
	f->parent = ino;
	f->link = NULL;
	
	if(nodes->flags & ISO9660_FLAGS_DIRECTORY) {
		f->readdir = iso9660_readdir;
		f->finddir = iso9660_finddir;
		f->mode = S_IFDIR;

		iso9660_dir_t* entry = (iso9660_dir_t*) kmalloc(ISO9660_SECTOR_SIZE);
		memset(entry, 0, ISO9660_SECTOR_SIZE);

		dev->position = iso9660_getlsb32(nodes->lba) * ISO9660_SECTOR_SIZE;
		fs_read(dev, entry, ISO9660_SECTOR_SIZE);

		f->userdata = (void*) entry;
	} else {
		f->read = iso9660_read;
		f->write = NULL;	/* Read Only */

		f->mode = S_IFREG;
		f->userdata = (void*) (iso9660_getlsb32(nodes->lba) * ISO9660_SECTOR_SIZE);
	}



	f->creat = NULL;
	f->rename = NULL;
	f->unlink = NULL;
	f->chown = NULL;
	f->flush = NULL;
	f->ioctl = NULL;

	
	kfree(snodes);
	return f;
}
