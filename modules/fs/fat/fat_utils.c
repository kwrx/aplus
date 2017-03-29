#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


const char* lfncpy(uint16_t* lfn, const char* name, size_t size) {
	const char* p = name;

	if(*p == 0) {
		memset(lfn, 0xFF, size * sizeof(uint16_t));
		return p;
	}


	while(size--) {
		if(*p)
			*lfn++ = *p & 0xFF;
		else {
			*lfn++ = 0;
			break;
		}

		p++;
	}
	
	return p;		
}

void lfncat(const char* name, uint16_t* lfn, size_t size) {
	while(size--) {
		if(*lfn == 0xFFFF)
			return;

		char p = *lfn & 0xFF;
		strncat(name, &p, 1);
		lfn++;
	}
}

void fatcat(const char* name, char* fatnm, char* fatex) {

	int i = 7;
	while(fatnm[i] == ' ')
		i--;


	char *p = (char*) &name[strlen(name)];

	int n;
	for(n = 0; n <= i; n++, p++, fatnm++)
		*p = *fatnm >= 'A' && *fatnm <= 'Z'
			? *fatnm + 32
			: *fatnm
			;

	if(strncmp(fatex, "   ", 3) == 0)
		return;

	i = 2;
	while(fatex[i] == ' ')
		i--;

	*p++ = '.';

	for(n = 0; n <= i; n++, p++, fatex++)
		*p = *fatex >= 'A' && *fatex <= 'Z'
			? *fatex + 32
			: *fatex
			;
}


void fat_update_FAT(fat_t* fat) {
	fat->dev->position = fat->first_fat_sector * fat->bytes_per_sector;
	if(vfs_write(fat->dev, fat->FAT, fat->fat_size * fat->bytes_per_sector) != (fat->fat_size * fat->bytes_per_sector))
		kprintf(ERROR, "[FAT] Error while updating FAT Table\n");
}




uint32_t fat_get_cluster(fat_t* fat, int index) {
	uint32_t next_cluster = 0;
	uint32_t active_cluster = index;

	switch(fat->type) {
		case FAT12:
			next_cluster = *(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))];
			
			if(active_cluster & 1)
				next_cluster >>= 4;
			else
				next_cluster &= 0x0FFF;

			if(next_cluster == 0xFF7)
				return FAT_BAD_CLUSTER;

			if(next_cluster >= 0xFF8)
				return FAT_END_CLUSTER;

			if(next_cluster == 0)
				return FAT_UNUSED_CLUSTER;

			return next_cluster;
		case FAT16:
			next_cluster = *(uint16_t*) &fat->FAT[(active_cluster * 2)];
			
		
			if(next_cluster == 0xFFF7)
				return FAT_BAD_CLUSTER;

			if(next_cluster >= 0xFFF8)
				return FAT_END_CLUSTER;

			if(next_cluster == 0)
				return FAT_UNUSED_CLUSTER;

			return next_cluster;
		case FAT32:
			next_cluster = *(uint32_t*) &fat->FAT[(active_cluster * 4)];
			next_cluster &= 0x0FFFFFFF;

		
			if(next_cluster == 0xFFFFFF7)
				return FAT_BAD_CLUSTER;

			if(next_cluster >= 0xFFFFFF8)
				return FAT_END_CLUSTER;

			if(next_cluster == 0)
				return FAT_UNUSED_CLUSTER;

			return next_cluster;
	}

	return FAT_END_CLUSTER;
}



void fat_set_cluster(fat_t* fat, int index, uint32_t value) {
	uint32_t next_cluster = 0;
	uint32_t active_cluster = index;

	switch(fat->type) {
		case FAT12:
			next_cluster = *(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))];
			
			if(active_cluster & 1)
				next_cluster >>= 4;
			else
				next_cluster &= 0x0FFF;

			switch(value) {
				case FAT_BAD_CLUSTER:
					next_cluster = 0xFF7;
					break;
				case FAT_END_CLUSTER:
					next_cluster = 0xFF8;
					break;
				case FAT_UNUSED_CLUSTER:
					next_cluster = 0;
					break;
				default:
					next_cluster = (value & 0xFFF);
					break;
			}


			if(active_cluster & 1) {
				*(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))] &= 0xFFF0;
				*(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))] |= (next_cluster << 4);
			} else {
				*(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))] &= 0x0FFF;
				*(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))] |= (next_cluster & 0x0FFF);
			}
			return;
		case FAT16:
			next_cluster = *(uint16_t*) &fat->FAT[(active_cluster * 2)];
			
			switch(value) {
				case FAT_BAD_CLUSTER:
					next_cluster = 0xFFF7;
					break;
				case FAT_END_CLUSTER:
					next_cluster = 0xFFF8;
					break;
				case FAT_UNUSED_CLUSTER:
					next_cluster = 0;
					break;
				default:
					next_cluster = (value & 0xFFFF);
					break;
			}


			*(uint16_t*) &fat->FAT[(active_cluster * 2)] = next_cluster;
			return;
		case FAT32:
			next_cluster = *(uint32_t*) &fat->FAT[(active_cluster * 4)];
			next_cluster &= 0x0FFFFFFF;

			switch(value) {
				case FAT_BAD_CLUSTER:
					next_cluster = 0xFFFFFF7;
					break;
				case FAT_END_CLUSTER:
					next_cluster = 0xFFFFFF8;
					break;
				case FAT_UNUSED_CLUSTER:
					next_cluster = 0;
					break;
				default:
					next_cluster = (value & 0x0FFFFFFF);
					break;
			}
		
			*(uint32_t*) &fat->FAT[(active_cluster * 4)] = next_cluster;
			return;
	}
}


uint32_t fat_next_sector(fat_t* fat, uint32_t sector) {
	uint32_t v = fat_get_cluster(fat, SECTOR_TO_CLUSTER(fat, sector));

	switch(v) {
		case FAT_BAD_CLUSTER:
		case FAT_END_CLUSTER:
		case FAT_UNUSED_CLUSTER:
			return v;
	}

	return CLUSTER_TO_SECTOR(fat, v);
}


int fat_check_entry(fat_t* fat, int* entry) {
	if(*entry >= 16) {
		uint32_t next_sector = fat_next_sector(fat, fat->dev->position / fat->bytes_per_sector - 1);

		switch(next_sector) {
			case FAT_BAD_CLUSTER:
			case FAT_END_CLUSTER:
			case FAT_UNUSED_CLUSTER:
				return E_ERR;
		}

		*entry = 0;
		fat->dev->position = next_sector * fat->bytes_per_sector;
	}

	*entry += 1;
	return E_OK;
}

int fat_alloc_cluster(fat_t* fat) {
	int i = 2;
	for(i = 0; i < fat->total_clusters; i++) {
		if(fat_get_cluster(fat, i) == FAT_UNUSED_CLUSTER)
			return i;
	}
	
	return 0;
}

int fat_alloc_sector(fat_t* fat, int end) {
	int sec = fat_alloc_cluster(fat);
	if(sec == 0)
		return 0;

	if(end) {
		int nend = fat_alloc_cluster(fat);
		if(nend == 0)
			return 0;

		fat_set_cluster(fat, sec, nend);
		fat_set_cluster(fat, nend, FAT_END_CLUSTER);
	}

	return CLUSTER_TO_SECTOR(fat, sec);
}


