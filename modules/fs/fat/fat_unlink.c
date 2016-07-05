#include <xdev.h>
#include <xdev/debug.h>
#include <xdev/vfs.h>
#include <xdev/mm.h>
#include <xdev/task.h>
#include <xdev/timer.h>
#include <libc.h>

#include "fat.h"


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
			return E_ERR;
		}

		if(e->name[0] == '\0')
			break;

		child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
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
					return E_ERR;
				}

				if(vfs_read(fat->dev, &buf, 32) != 32) {
					kfree((void*) child->name);
					kfree((void*) child);

					errno = EIO;
					return E_ERR;
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

	

		if(strcmp(child->name, name) != 0) {
			kfree((void*) child->name);
			kfree((void*) child);

			continue;
		}


		e->name[0] = 0xE5;
		
		fat->dev->position -= 32;
		if(vfs_write(fat->dev, &buf, 32) != 32) {
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
	} while(1);

	errno = ENOENT;
	return E_ERR;
}
