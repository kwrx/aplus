#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"



char* fatutils_extract_name(char* bufname, int* r, fat_entry_t* e) {
    null_if(!r || !e, EINVAL);


    if((*r) == FAT_EXTRACT_DONE) {
        bufname = (char*) kmalloc(FAT_MAXFN, GFP_KERNEL);
        null_if(!bufname, ENOMEM);

        memset(bufname, 0, FAT_MAXFN);
    }


    switch(e->name[0]) {
        case '\0':
            *r = FAT_EXTRACT_END;
            return NULL;
        case '\xE5':
            memset(bufname, 0, FAT_MAXFN);
            *r = FAT_EXTRACT_CONTINUE;
            return bufname;
        default:
            break;
    }


    if(e->flags == ATTR_LFN) {
        fat_entry_lfn_t* lfn = (fat_entry_lfn_t*) e;

        lfncat(bufname, lfn->name_2, 2);
        lfncat(bufname, lfn->name_1, 6);
        lfncat(bufname, lfn->name_0, 5);

        *r = FAT_EXTRACT_CONTINUE;
        return bufname;
    }

    if(bufname[0] == '\0')
        fatcat(bufname, e->name, e->extension);

    *r = FAT_EXTRACT_DONE;
    return bufname;
}



void fatutils_new_child(fat_t* fat, fat_entry_t* e, uint32_t entry_offset, inode_t** childptr, inode_t* parent) {
    return_if(!childptr || !fat || !e, EINVAL);

    inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_KERNEL);
    return_if(!child, ENOMEM);
    memset(child, 0, sizeof(inode_t));

    fat_t* fc = (fat_t*) kmalloc(sizeof(fat_t), GFP_KERNEL);
    return_if(!fc, ENOMEM);
    memcpy(fc, fat, sizeof(fat_t));

    
    child->userdata = (void*) fc;
    child->ino = vfs_inode();
    child->mode = (e->flags & ATTR_DIRECTORY ? S_IFDIR : S_IFREG | S_IXUSR | S_IXGRP | S_IXOTH) |
				    (e->flags & ATTR_RDONLY ? 0444 : 0666) & ~current_task->umask;
    
    child->dev =
    child->rdev =
    child->nlink = 0;

    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->size = (off64_t) e->size;

    child->atime = 
    child->ctime = 
    child->mtime = timer_gettimestamp();

    child->parent = parent;
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
        child->read = fat_read;
        child->write = fat_write;
    }
    
    child->chown = NULL;
    child->chmod = NULL;
    child->ioctl = NULL;
			
    
    fc->entry_cluster = (e->cluster_high << 16) | (e->cluster_low & 0xFFFF);
    fc->entry_offset = entry_offset;
    *childptr = child;
}



int fatutils_next_cluster(fat_t* fat, int active_cluster) {
    int next_cluster = 0;

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


int fatutils_alloc_cluster(fat_t* fat, int active_cluster, int* ncluster) {
    int i;
	switch(fat->type) {
		case FAT12:
            break;
		case FAT16:
            for(i = 0; i < fat->fat_size * fat->bytes_per_sector; i += 2) {
                if((*(uint16_t*) &fat->FAT[i]) != 0)
                    continue;
                
                if(active_cluster != -1)
                    *(uint16_t*) &fat->FAT[(active_cluster * 2)] = i >> 1;

                *(uint16_t*) &fat->FAT[i] = 0xFFF8;
                *ncluster = i >> 1;

                return E_OK;            
            }
            break;
		case FAT32:
            for(i = 0; i < fat->fat_size * fat->bytes_per_sector; i += 4) {
                if((*(uint32_t*) &fat->FAT[i] & 0x0FFFFFFF) != 0)
                    continue;
                
                if(active_cluster != -1)
                    *(uint32_t*) &fat->FAT[(active_cluster * 4)] = (i >> 2) & 0x0FFFFFFF;

                *(uint32_t*) &fat->FAT[i] = 0x0FFFFFF8;
                *ncluster = i >> 2;

                return E_OK;
            }
            break;
	}

	errno = ENOSPC;
	return E_ERR;
}

int fatutils_free_cluster(fat_t* fat, int active_cluster) {
    int i;
	switch(fat->type) {
		case FAT12:
            i = *(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))];
            *(uint16_t*) &fat->FAT[(active_cluster + (active_cluster / 2))] = 0;

            if(active_cluster & 1)
				i >>= 4;
			else
				i &= 0x0FFF;

            if(i == 0xFF7)
				i = FAT_BAD_CLUSTER;

			if(i >= 0xFF8)
				i = FAT_END_CLUSTER;

            break;
		case FAT16:
            i = *(uint16_t*) &fat->FAT[(active_cluster * 2)];
            *(uint16_t*) &fat->FAT[(active_cluster * 2)] = 0;

            if(i == 0xFFF7)
				i = FAT_BAD_CLUSTER;

			if(i >= 0xFFF8)
				i = FAT_END_CLUSTER;

            break;
		case FAT32:
            i = *(uint16_t*) &fat->FAT[(active_cluster * 4)] & 0x0FFFFFFF;
            *(uint16_t*) &fat->FAT[(active_cluster * 4)] = 0;

            if(i == 0xFFFFFF7)
				i = FAT_BAD_CLUSTER;

			if(i >= 0xFFFFFF8)
				i = FAT_END_CLUSTER;

            break;
	}

    return i;
}


int fatutils_update_entry(fat_t* fat, struct inode* inode, int delete) {
    fat_entry_t e;

    fat->dev->position = (off64_t) fat->entry_offset;
    fail_if(vfs_read(fat->dev, &e, sizeof(e)) != sizeof(e), EIO);

    e.cluster_high = (fat->entry_cluster >> 16) & 0xFFFF;
    e.cluster_low = (fat->entry_cluster) & 0xFFFF;
    e.size = (uint32_t) inode->size;

    if(delete)
        e.name[0] = '\xE5';

    /* TODO: time, ecc... */

    fat->dev->position = (off64_t) fat->entry_offset;
    fail_if(vfs_write(fat->dev, &e, sizeof(e)) != sizeof(e), EIO);
    return E_OK;
}

int fatutils_update_fat(fat_t* fat) {
    fat->dev->position = fat->first_fat_sector * fat->bytes_per_sector;
    fail_if(vfs_write(fat->dev, fat->FAT, fat->fat_size * fat->bytes_per_sector) != (fat->fat_size * fat->bytes_per_sector), EIO);

    return E_OK;
}



void lfncat(char* name, uint16_t* lfn, size_t size) {
    int i;
    for(i = strlen(name) + 1; i >= 0; i--)
        name[i + size] = name[i];

	for(i = 0; i < size; i++) {
		if(*lfn == 0xFFFF)
			return;
    
		name[i] = *lfn & 0xFF;
		lfn++;
	}
}

void fatcat(char* name, char* fatnm, char* fatex) {

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


