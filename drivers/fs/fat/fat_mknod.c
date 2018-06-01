/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "fat.h"


struct inode* fat_mknod(struct inode* inode, char* name, mode_t mode) {
    if(unlikely(!inode || !inode->userdata)) {
        errno = EINVAL;
        return NULL;
    }

    int i;
    fat_t* fat = (fat_t*) inode->userdata;

    struct fat_dir_entry e;
    if(fatfs_get_file_entry(fat->sb, fat->cluster, name, &e) == 1) {
        errno = EEXIST;
        return NULL;
    }

    uint32_t cluster = 0;
    if(!fatfs_allocate_free_space(fat->sb, 1, &cluster, 1)) {
        errno = ENOSPC;
        return NULL;
    }

    uint8_t ss[FAT_SECTOR_SIZE];
    memset(ss, 0, sizeof(ss));

    
    if(S_ISDIR(mode)) {
        for(i = 0; i < fat->sb->sectors_per_cluster; i++) {
            if(!fatfs_write_sector(fat->sb, cluster, i, ss)) {
                errno = EIO;
                return NULL;
            }
        }
    }


    char sfn[FAT_SFN_SIZE_FULL];
    char buf[FAT_SFN_SIZE_FULL + 2];

    for(i = 0; i < 9999; i++) {
        fatfs_lfn_create_sfn(sfn, name);

        if(i != 0)
            fatfs_lfn_generate_tail(buf, sfn, i);
        else
            memcpy(buf, sfn, FAT_SFN_SIZE_FULL);

        if(fatfs_sfn_exists(fat->sb, fat->cluster, buf) == 0)
            break;
    }

    if(i == 9999) {
        fatfs_free_cluster_chain(fat->sb, cluster);
        errno = ENOSPC;
        return NULL;
    }

    if(!fatfs_add_file_entry(fat->sb, fat->cluster, name, buf, cluster, 0, S_ISDIR(mode) ? 1 : 0)) {
        fatfs_free_cluster_chain(fat->sb, cluster);
        errno = ENOSPC;
        return NULL;
    }


    inode_t* child = fat_mkchild(inode, NULL, name, mode);
    if(unlikely(!child))
        return NULL;

    fat_t* cfs = (fat_t*) child->userdata;
    cfs->cluster = cluster;

    fatfs_fat_purge(cfs->sb);
    return child;
}