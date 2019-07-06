/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
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
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <errno.h>

#include "ext2.h"



__thread_safe
ssize_t ext2_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);
    DEBUG_ASSERT(count);

    struct vfs_sb* sb = smartptr_get(inode->sb);
    struct ientry* ino = smartptr_get(inode->ino);

    DEBUG_ASSERT(sb->fsinfo);
    DEBUG_ASSERT(sb->fsid == EXT2_ID);


    ext2_t* ext2 = (ext2_t*) sb->fsinfo;


    struct ext2_inode node;
    ext2_utils_read_inode(ext2, ino->st.st_ino, &node);


    int entries = 0;
    int q;

    for(q = 0; q < node.i_size; q += ext2->blocksize) {

        __lock(&ext2->lock, {
        

            ext2_utils_read_inode_data(ext2, node.i_block, q / ext2->blocksize, 0, ext2->cache, ext2->blocksize);

            int i;
            for(i = 0; i < ext2->blocksize; ) {

                struct ext2_dir_entry_2* d = (struct ext2_dir_entry_2*) ((uintptr_t) ext2->cache + i);

                if(pos > 0)
                    pos--;

                else {

                    if(count == 0)
                        break;


                    e->d_ino = d->inode;
                    e->d_off = i;
                    e->d_reclen = sizeof(struct dirent);
                    e->d_type = 0;
                    e->d_name[d->name_len] = '\0';
                    
                    strncpy(e->d_name, d->name, d->name_len);


                    if(ext2->sb.s_rev_level == EXT2_DYNAMIC_REV)
                        e->d_type = d->file_type;


                    e++;
                    entries++;
                    count--;
                }
                
                i += d->rec_len;
            }

        });


        if(count == 0)
            break;

    }

    return entries;
}
