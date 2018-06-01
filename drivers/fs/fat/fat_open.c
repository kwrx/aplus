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



int fat_open(struct inode* inode) {
    if(unlikely(!inode || !inode->userdata)) {
        errno = EINVAL;
        return -1;
    }

    fat_t* fat = (fat_t*) inode->userdata;
    
    struct lfn_cache lfn;
    fatfs_lfn_cache_init(&lfn, 0);



    int sector = 0;
    inode_t* child = NULL;

    do {
        if(fatfs_sector_reader(fat->sb, fat->cluster, sector, NULL)) {

            for(int item = 0; item < FAT_DIR_ENTRIES_PER_SECTOR; item++) {
                uint16_t off = (FAT_DIR_ENTRY_SIZE * item);
                struct fat_dir_entry* e = (struct fat_dir_entry*) (fat->sb->currentsector.sector + off);

                if(fatfs_entry_lfn_text(e))
                    fatfs_lfn_cache_entry(&lfn, fat->sb->currentsector.sector + off);
                else if(fatfs_entry_lfn_invalid(e))
                    fatfs_lfn_cache_init(&lfn, 0);

                else {
                    char* fn = NULL;
                    if(fatfs_entry_lfn_exists(&lfn, e))
                        fn = fat_getname(e, fatfs_lfn_cache_get(&lfn));
                    else if(fatfs_entry_sfn_only(e))
                        fn = fat_getname(e, NULL);
                    else
                        continue;


                    if (
                        strncmp(fn, ".", 1) == 0 ||
                        strncmp(fn, "..", 2) == 0) {
                            
                        kfree(fn);
                        continue;
                    }
                    
                    struct inode_childs* cx;
                    for(cx = inode->childs; cx; cx = cx->next)
                        if(strcmp(cx->inode->name, fn) == 0)
                            break;

                    if(unlikely(cx)) {
                        kfree(fn);
                        continue;
                    }


                    inode_t* child = fat_mkchild(inode, e, fn, 0);
                    if(!child)
                        continue;

                    cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
                    cx->inode = child;
                    cx->next = inode->childs;
                    inode->childs = cx;

                    fatfs_lfn_cache_init(&lfn, 0);
                }
            }

            sector++;
        } else
            break;
    } while(1);
    

    return 0;
}
