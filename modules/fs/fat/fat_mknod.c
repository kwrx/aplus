#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"

static uint8_t __cksum(uint8_t* buf, size_t size) {
	uint8_t c = 0;
	while(size--) {
		c = ((c & 0xFF) >> 1) + ((c & 1) << 7);
		c = ((c + *buf++) & 0xFF);
	}

	return c;
}


struct inode* fat_mknod(struct inode* inode, char* name, mode_t mode) {
	fat_t* fat = (fat_t*) inode->userdata;
	if(unlikely(!fat)) {
		errno = EINVAL;
		return NULL;
	}	

	
	if(fat->entry_sector == 0) {
		if((fat->entry_sector = fat_alloc_sector(fat, 1)) == 0) {
			errno = ENOSPC;
			return NULL;
		}
	}

	fat->dev->position = fat->entry_sector * fat->bytes_per_sector;
	inode_t* child = NULL;


	
	static char buf[512];
	fat_entry_t* e = (fat_entry_t*) &buf;
	fat_entry_lfn_t* lfn = (fat_entry_lfn_t*) &buf;

	int entry = 0;


	do {

		if(unlikely(fat_check_entry(fat, &entry) == E_ERR))
			break;

		if(vfs_read(fat->dev, &buf, 32) != 32) {
			errno = EIO;
			return NULL;
		}


		if(e->name[0] == '\0') {
			fat->dev->position -= 32;		
			break;
		}
		

		if(e->flags == ATTR_LFN) {
			do {
				if(unlikely(fat_check_entry(fat, &entry) == E_ERR))
					break;

				if(vfs_read(fat->dev, &buf, 32) != 32) {
					errno = EIO;
					return NULL;
				}
			} while(lfn->flags == ATTR_LFN);
		}
	} while(1);

	kprintf(INFO, "GOOD at 0x%x\n", fat->dev->position);
	

	uint32_t sector = fat->dev->position / fat->bytes_per_sector;
	uint32_t cluster = fat_get_cluster(fat, SECTOR_TO_CLUSTER(fat, sector));

	uint32_t nc;
	if(cluster == FAT_END_CLUSTER) {
		if((nc = fat_alloc_cluster(fat))) {
			fat_set_cluster(fat, cluster, nc);
			fat_set_cluster(fat, nc, FAT_END_CLUSTER);
		} else {
			errno = ENOSPC;
			return NULL;
		}

		entry = 0;
	}

	if(unlikely(fat_check_entry(fat, &entry) == E_ERR)) {
		kprintf(INFO, "WTF\n");
		return NULL;
	}

	int count = (strlen(name) / 13 + 1);
	lfn = (fat_entry_lfn_t*) kmalloc(sizeof(fat_entry_lfn_t) * count, GFP_ATOMIC);
	e = (fat_entry_t*) kmalloc(sizeof(fat_entry_t), GFP_ATOMIC);

	if(unlikely(!lfn || !e)) {
		errno = ENOMEM;
		return NULL;
	}


	const char* p = name;
	while(count--) {
		p = lfncpy(lfn[count].name_0, p, 5);
		p = lfncpy(lfn[count].name_1, p, 6);
		p = lfncpy(lfn[count].name_2, p, 2);

		lfn[count].order = count;
		lfn[count].flags = ATTR_LFN;
		lfn[count].let = 0;
		lfn[count].cksum = 0;
		lfn[count].null = 0;

		lfn[count].cksum = __cksum((uint8_t*) &lfn[count], sizeof(fat_entry_lfn_t));
	}

	count = (strlen(name) / 13 + 1);

	lfn[0].order = 'A' + count - 1;



	memset(e, 0, sizeof(fat_entry_t));
	memset(e->name, 0x20, 8);
	memset(e->extension, 0x20, 3);
	
	e->flags = S_ISDIR(mode) ? ATTR_DIRECTORY : 0;
	
	p = name;
	int j;
	for(j = 0; *p && j < 6; j++, p++)
		e->name[j] = *p >= 'a' && *p <= 'z' ? *p - 32 : *p;

	e->name[j++] = '~';
	e->name[j++] = count > 1 ? '0' + lfn[count - 1].order : 'A';



	int i;
	for(i = 0; i <= count; i++) {
		if(unlikely(fat_check_entry(fat, &entry) == E_ERR)) {
			uint32_t sector = fat->dev->position / fat->bytes_per_sector;
			uint32_t cluster = fat_get_cluster(fat, SECTOR_TO_CLUSTER(fat, sector));

	
			if((nc = fat_alloc_cluster(fat))) {
				fat_set_cluster(fat, cluster, nc);
				fat_set_cluster(fat, nc, FAT_END_CLUSTER);
			} else {
				kfree(lfn);
				kfree(e);

				errno = ENOSPC;
				return NULL;
			}
		}

		if(i == count) {
			if(vfs_write(fat->dev, e, sizeof(fat_entry_t)) != sizeof(fat_entry_t)) {
				kfree(lfn);
				kfree(e);

				errno = EIO;
				return NULL;
			}
		} else {
			if(vfs_write(fat->dev, &lfn[i], sizeof(fat_entry_lfn_t)) != sizeof(fat_entry_lfn_t)) {
				kfree(lfn);
				kfree(e);

				errno = EIO;
				return NULL;
			}
		}
	}


	kfree(lfn);
	kfree(e);

	child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
	if(unlikely(!child)) {
		errno = ENOMEM;
		return NULL;
	}

	memset(child, 0, sizeof(inode_t));

	fat_t* fc = (fat_t*) kmalloc(sizeof(fat_t), GFP_USER);
	if(unlikely(!fc)) {
		kfree((void*) child);

		errno = ENOMEM;
		return NULL;
	}

	memcpy(fc, fat, sizeof(fat_t));
	fc->entry_sector = 0;


	child->name = strdup(name);
	child->userdata = (void*) fc;

	child->ino = vfs_inode();
	child->mode = mode & ~current_task->umask;

	child->dev =
	child->rdev =
	child->nlink = 0;

	child->uid = current_task->uid;
	child->gid = current_task->gid;
	child->size = (off64_t) 0;

	child->atime = 
	child->ctime = 
	child->mtime = timer_gettime();

	child->parent = inode;
	child->link = NULL;

	child->childs = NULL;


	if(e->flags & ATTR_DIRECTORY) {
		child->finddir = fat_finddir;
		child->mknod = fat_mknod;
		child->rename = NULL;
		child->unlink = fat_unlink;
		child->open = fat_open;
		child->close = fat_close;
	} else {
		child->read = NULL;
		child->write = NULL;
	}
	
	child->chown = NULL;
	child->chmod = NULL;
	child->ioctl = NULL;



	fat_update_FAT(fat);
	
	return child;
}
