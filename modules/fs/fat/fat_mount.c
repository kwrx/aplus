#include <aplus.h>
#include <aplus/base.h>
#include <aplus/vfs.h>
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/debug.h>

#include "fat.h"


int fat_mount(struct inode* dev, struct inode* dir) {
    fail_if(!dev || !dir, EINVAL);

    fat_mbr_t mbr;
    fail_if(vfs_read(dev, &mbr, 512) != 512, EIO);

    fat_t* fat = (fat_t*) kmalloc(sizeof(fat_t), GFP_KERNEL);
    fail_if(!fat, ENOMEM);


    fat->dev = dev;
	fat->bytes_per_sector = mbr.bytes_per_sector;

	fat->total_sectors = (mbr.total_sectors == 0)
				? mbr.total_sectors_large
				: mbr.total_sectors
				;


	fat->fat_size = (mbr.number_of_sector_per_fat == 0)
				? mbr.fat32.sectors_per_fat
				: mbr.number_of_sector_per_fat
				;


	KASSERT(mbr.bytes_per_sector);
	KASSERT(mbr.sector_per_cluster);

	fat->rootdir_sectors = ((mbr.rootdir_entries * 32) + (mbr.bytes_per_sector - 1)) / mbr.bytes_per_sector;
	fat->first_data_sector = mbr.reserved_sectors + (mbr.fat_count * fat->fat_size) + fat->rootdir_sectors;
	fat->first_fat_sector = mbr.reserved_sectors;

	fat->data_sectors = fat->total_sectors - (mbr.reserved_sectors + (mbr.fat_count * fat->fat_size) + fat->rootdir_sectors);
	fat->total_clusters = fat->data_sectors / mbr.sector_per_cluster;
	fat->sector_per_cluster = mbr.sector_per_cluster;


    if(fat->total_clusters < 4085)
		fat->type = FAT12;
	else if(fat->total_clusters < 65525)
		fat->type = FAT16;
	else if(fat->total_clusters < 268435445)
		fat->type = FAT32;
    else {
		kfree(fat);

		errno = EINVAL;
		return E_ERR;
	}


    fat->FAT = (uint8_t*) kmalloc(fat->fat_size * fat->bytes_per_sector, GFP_KERNEL);
    fail_if(!fat->FAT, ENOMEM);

    dev->position = fat->first_fat_sector * fat->bytes_per_sector;
    fail_if(vfs_read(dev, fat->FAT, fat->fat_size * fat->bytes_per_sector) != (fat->fat_size * fat->bytes_per_sector), EIO);


    

    switch(fat->type) {
		case FAT12:
		case FAT16:
			fat->entry_sector = fat->first_data_sector - fat->rootdir_sectors;

            dir->open = fat12_16_root_open;
            dir->finddir = fat12_16_root_finddir;
            dir->unlink = fat_unlink;
            dir->mknod = fat12_16_root_mknod;
			break;
		case FAT32:
			fat->entry_cluster = mbr.fat32.cluster_of_root;

            dir->open = fat_open;
            dir->finddir = fat_finddir;
            dir->unlink = fat_unlink;
            dir->mknod = NULL;//fat_mknod;
			break;
		default:
            kfree(fat->FAT);
			kfree(fat);

			errno = EINVAL;
			return E_ERR;
	}

    dir->close = fat_close;
    dir->rename = NULL;
    dir->userdata = (void*) fat;
    dir->childs = NULL;
    return E_OK;
}