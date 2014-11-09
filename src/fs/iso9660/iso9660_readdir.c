#include <aplus.h>
#include <aplus/fs.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include "iso9660.h"

struct dirent* iso9660_readdir(inode_t* ino, int index) {
	if(!ino)
		return NULL;
		
	if(!ino->dev)
		return NULL;

	if(!ino->userdata)
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

	/* Skip dots (".", "..") */
	nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);

	for(int i = 0; i < index; i++) {		
		if(nodes->size == 0) {		
			kfree(snodes);
			return NULL;
		}
		
		nodes = (iso9660_dir_t*) ((uint32_t) nodes + nodes->size);
	}


	if(nodes->size == 0) {
		kfree(snodes);
		return NULL;
	}

	struct dirent* ent = (struct dirent*) kmalloc(sizeof(struct dirent));
	memset(ent, 0, sizeof(struct dirent));
	
	strncpy(ent->d_name, nodes->reserved, nodes->idlen);	
	iso9660_checkname(ent->d_name);

	ent->d_ino = 0;

	kfree(snodes);
	return ent;
}
