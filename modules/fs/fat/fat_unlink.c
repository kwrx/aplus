#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"

static int new_child(inode_t** childptr) {
	inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
	if(unlikely(!child)) {
		errno = ENOMEM;
		return E_ERR;
	}

	memset(child, 0, sizeof(inode_t));



	child->name = (const char*) kmalloc(FAT_MAXFN, GFP_ATOMIC);
	if(unlikely(!child->name)) {
		kfree((void*) child);

		errno = ENOMEM;
		return E_ERR;
	}

	memset((void*) child->name, 0, FAT_MAXFN);

	if(childptr)
		*childptr = child;

	return E_OK;
}


int fat_unlink(struct inode* inode, char* name) {
	fat_t* fat = (fat_t*) inode->userdata;
	if(unlikely(!fat)) {
		errno = EINVAL;
		return E_ERR;
	}	

	if(fat->entry_sector == 0) {
		errno = ENOENT;	
		return E_ERR;
	}

	fat->dev->position = fat->entry_sector * fat->bytes_per_sector;
	inode_t* child;
	if(new_child(&child) == E_ERR)
		return E_ERR;

	static char buf[8192];
	int i;

	do {
		if(vfs_read(fat->dev, &buf, fat->bytes_per_sector * fat->sector_per_cluster) != fat->bytes_per_sector * fat->sector_per_cluster) {
			errno = EIO;
			return E_ERR;
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
			


			if(strcmp(child->name, name) != 0) {
				kfree((void*) child->name);
				kfree((void*) child);

				if(new_child(&child) == E_ERR)
					return E_ERR;

				continue;
			}


			e->name[0] = 0xE5;
		
			fat->dev->position -= fat->bytes_per_sector * fat->sector_per_cluster;
			fat->dev->position += i;
			if(vfs_write(fat->dev, &buf[i], 32) != 32) {
				errno = EIO;
				return E_ERR;
			}




	
			int cluster = (e->cluster_high << 16) | (e->cluster_low & 0xFFFF);
			if(likely(cluster)) {
				do {
					register int n = cluster;
					if((cluster = fat_get_cluster(fat, cluster)) == FAT_BAD_CLUSTER)
						break;

					fat_set_cluster(fat, n, FAT_UNUSED_CLUSTER);
				} while(cluster != FAT_END_CLUSTER);


				if(likely(cluster == FAT_END_CLUSTER))
					fat_set_cluster(fat, cluster, FAT_UNUSED_CLUSTER);


				fat_update_FAT(fat);
			}




			kfree((void*) child->name);
			kfree((void*) child);

			return E_OK;
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

	errno = ENOENT;
	return E_ERR;
}
