#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>

#include "fat.h"


struct inode* fat_finddir(struct inode* inode, char* name) {
	fat_t* fat = (fat_t*) inode->userdata;
	if(unlikely(!fat)) {
		errno = EINVAL;
		return NULL;
	}	


	if(fat->entry_sector == 0) {
		errno = ENOENT;	
		return NULL;
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


		if(e->name[0] == '\0')
			break;

		child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
		if(unlikely(!child)) {
			errno = ENOMEM;
			return NULL;
		}

		memset(child, 0, sizeof(inode_t));



		child->name = (const char*) kmalloc(FAT_MAXFN, GFP_ATOMIC);
		if(unlikely(!child->name)) {
			kfree((void*) child);

			errno = ENOMEM;
			return NULL;
		}

		memset((void*) child->name, 0, FAT_MAXFN);




		if(e->flags == ATTR_LFN) {
			do {
#if CONFIG_LFN
				lfncat(child->name, lfn->name_2, 2);
				lfncat(child->name, lfn->name_1, 6);
				lfncat(child->name, lfn->name_0, 5);
#endif

				if(unlikely(fat_check_entry(fat, &entry) == E_ERR)) {
					kfree((void*) child->name);
					kfree((void*) child);

					errno = ENOENT;
					return NULL;
				}

				if(vfs_read(fat->dev, &buf, 32) != 32) {
					kfree((void*) child->name);
					kfree((void*) child);

					errno = EIO;
					return NULL;
				}
			} while(lfn->flags == ATTR_LFN);
		}


		if(e->name[0] == '\xE5') {
			kfree((void*) child->name);
			kfree((void*) child);

			continue;
		}

		if(child->name[0] == '\0')
			fatcat(child->name, e->name, e->extension);
		

		kprintf(INFO, "FINDDIR: %s == %s\n", child->name, name);

		if(strcmp(child->name, name) != 0) {
			kfree((void*) child->name);
			kfree((void*) child);

			continue;
		}



		fat_t* fc = (fat_t*) kmalloc(sizeof(fat_t), GFP_USER);
		if(unlikely(!fc)) {
			kfree((void*) child->name);
			kfree((void*) child);

			errno = ENOMEM;
			return NULL;
		}

		memcpy(fc, fat, sizeof(fat_t));

		int cluster = (e->cluster_high << 16) | (e->cluster_low & 0xFFFF);
		fc->entry_sector = cluster
					? CLUSTER_TO_SECTOR(fc, cluster) 
					: 0
					;




		child->userdata = (void*) fc;

		child->ino = vfs_inode();
		child->mode = (e->flags & ATTR_DIRECTORY ? S_IFDIR : S_IFREG) |
				(e->flags & ATTR_RDONLY ? 0444 : 0666) & ~current_task->umask;

		child->dev =
		child->rdev =
		child->nlink = 0;

		child->uid = current_task->uid;
		child->gid = current_task->gid;
		child->size = (off64_t) e->size;

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
		
		return child;	
	} while(1);

	errno = ENOENT;
	return NULL;
}
