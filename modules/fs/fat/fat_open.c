#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


#define TRACE(x)																		\
	{																					\
		kprintf(ERROR, "fat: (%d):%s (): %s\n", __LINE__, __func__, #x);				\
		x;																				\
	}


#define VTRACE(y, x)																	\
	{																					\
		kprintf(ERROR, "fat: (%d):%s (): %s = " y "\n", __LINE__, __func__, #x, x);		\
	}


static int new_child(inode_t** childptr) {
	inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
	if(unlikely(!child)) {
		errno = ENOMEM;
		TRACE(return E_ERR);
	}

	memset(child, 0, sizeof(inode_t));



	child->name = (const char*) kmalloc(FAT_MAXFN, GFP_ATOMIC);
	if(unlikely(!child->name)) {
		kfree((void*) child);

		errno = ENOMEM;
		TRACE(return E_ERR);
	}

	memset((void*) child->name, 0, FAT_MAXFN);

	if(childptr)
		*childptr = child;

	return E_OK;
}





int fat_open(struct inode* inode) {
	fat_t* fat = (fat_t*) inode->userdata;
	if(unlikely(!fat)) {
		errno = EINVAL;
		TRACE(return E_ERR);
	}	


	if(fat->entry_sector == 0)
		return E_OK;

	fat->dev->position = fat->entry_sector * fat->bytes_per_sector;


	inode_t* child;
	if(new_child(&child) == E_ERR)
		TRACE(return E_ERR);

	static char buf[8192];
	int i;

	do {
		if(vfs_read(fat->dev, &buf, fat->bytes_per_sector * fat->sector_per_cluster) != fat->bytes_per_sector * fat->sector_per_cluster) {
			errno = EIO;
			TRACE(return E_ERR);
		}


		for(i = 0; i < fat->bytes_per_sector * fat->sector_per_cluster; i += 32) {
			fat_entry_t* e = (fat_entry_t*) &buf[i];
			fat_entry_lfn_t* lfn = (fat_entry_lfn_t*) &buf[i];


			if(e->name[0] == '\0')
				return E_OK;

			if(e->name[0] == '\xE5') {				
				kfree((void*) child->name);
				kfree((void*) child);

				if(new_child(&child) == E_ERR)
					return E_ERR;
				continue;
			}


			if(e->flags == ATTR_LFN) {
#if CONFIG_LFN
				lfncat(child->name, lfn->name_0, 5);
				lfncat(child->name, lfn->name_1, 6);
				lfncat(child->name, lfn->name_2, 2);
#endif
				continue;
			}
	

			if(child->name[0] == '\0')
				fatcat(child->name, e->name, e->extension);
			

			int cc = 0;
			struct inode_childs* cx;
			for(cx = inode->childs; cx; cx = cx->next) {
				if(strcmp(cx->inode->name, child->name) == 0) {
					kfree((void*) child->name);
					kfree((void*) child);

					if(new_child(&child) == E_ERR)
						TRACE(return E_ERR);
					
					cc++;
					break;
				}
			}

			if(cc)
				continue;


			fat_t* fc = (fat_t*) kmalloc(sizeof(fat_t), GFP_USER);
			if(unlikely(!fc)) {
				kfree((void*) child->name);
				kfree((void*) child);

				errno = ENOMEM;
				TRACE(return E_ERR);
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
			


			

			cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
			cx->inode = child;
			cx->next = inode->childs;
			inode->childs = cx;


			if(new_child(&child) == E_ERR)
				TRACE(return E_ERR);		
		}
		

		int e = fat_next_sector(fat, fat->dev->position / fat->bytes_per_sector - 1);
		switch(e) {
			case FAT_BAD_CLUSTER:
				kprintf(ERROR, "fat: BAD_CLUSTER! %d\n", e * fat->bytes_per_sector);
			case FAT_END_CLUSTER:
			case FAT_UNUSED_CLUSTER:
				return E_OK;
			default:
				fat->dev->position = e * fat->bytes_per_sector;
		}
	} while(1);

	return E_OK;
}
