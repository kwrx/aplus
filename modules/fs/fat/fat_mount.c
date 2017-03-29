#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <libc.h>

#include "fat.h"

int fat_mount(struct inode* dev, struct inode* dir) {
	fat_mbr_t mbr;
	if(vfs_read(dev, &mbr, 512) != 512) {
		errno = EIO;	
		return E_ERR;
	}

	
	fat_t* fat = (fat_t*) kmalloc(sizeof(fat_t), GFP_ATOMIC);
	if(unlikely(!fat)) {
		errno = ENOMEM;
		return E_ERR;
	}

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

	fat->rootdir_sectors = ((mbr.dir_entries * 32) + (mbr.bytes_per_sector - 1)) / mbr.bytes_per_sector;
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


	switch(fat->type) {
		case FAT12:
		case FAT16:
			fat->entry_sector = fat->first_data_sector - fat->rootdir_sectors;
			break;
		case FAT32:
			fat->entry_sector = CLUSTER_TO_SECTOR(fat, mbr.fat32.cluster_of_root);
			break;
		default:
			kfree(fat);

			errno = EINVAL;
			return E_ERR;
	}


	fat->FAT = (uint8_t*) kmalloc(fat->fat_size * fat->bytes_per_sector, GFP_ATOMIC);
	if(unlikely(!fat->FAT)) {
		kfree(fat);

		errno = ENOMEM;
		return E_ERR;
	}


	dev->position = fat->first_fat_sector * fat->bytes_per_sector;
	if(vfs_read(dev, fat->FAT, fat->fat_size * fat->bytes_per_sector) != (fat->fat_size * fat->bytes_per_sector)) {
		kfree(fat->FAT);	
		kfree(fat);

		errno = EIO;
		return E_ERR;
	}


	dir->userdata = (void*) fat;
	dir->open = fat_open;
	dir->close = fat_close;
	dir->finddir = fat_finddir;
	dir->unlink = fat_unlink;
	dir->mknod = fat_mknod;

	return E_OK;
}
