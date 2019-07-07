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
inode_t* ext2_finddir(inode_t* inode, const char * name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == EXT2_ID);
    DEBUG_ASSERT(name);

    ext2_t* ext2 = (ext2_t*) inode->sb->fsinfo;



    struct ext2_inode* n = vfs_cache_get(&inode->sb->cache, inode->ino);
 


    int q;
    for(q = 0; q < n->i_size; q += ext2->blocksize) {


        __lock(&ext2->lock, {
        
            ext2_utils_read_inode_data(ext2, n->i_block, q / ext2->blocksize, 0, ext2->cache, ext2->blocksize);


            int i;
            for(i = 0; i < ext2->blocksize; ) {

                struct ext2_dir_entry_2* e = (struct ext2_dir_entry_2*) ((uintptr_t) ext2->cache + i);


                /* Found? */
                if(strncmp(name, e->name, e->name_len) == 0) {

                    struct ext2_inode* n = vfs_cache_get(&inode->sb->cache, e->inode);


                    inode_t* d = (inode_t*) kcalloc(sizeof(inode_t), 1, GFP_KERNEL); /* FIXME: Add Cache */

                    d->

                    strncpy(d->name, n->name, MAXNAMLEN);


                    d->ops.getaddr = ext2_getattr;
                    //d->ops.setattr = ext2_setattr;
                    //d->ops.fsync = ext2_fsync;

                    if(S_ISDIR(n->i_mode)) {

                        //d->ops.creat   = ext2_creat;
                        d->ops.finddir = ext2_finddir;
                        d->ops.readdir = ext2_readdir;
                        //d->ops.rename  = ext2_rename;
                        //d->ops.symlink = ext2_symlink;
                        //d->ops.unlink  = ext2_unlink;

                    }

                    if(S_ISREG(n->i_mode)) {

                        //d->ops.truncate = ext2_truncate;
                        d->ops.read = ext2_read;
                        //d->ops.write = ext2_write;

                    }

                    if(S_ISLNK(n->i_mode)) {

                        d->ops.readlink = ext2_readlink;

                    }
                

                    spinlock_init(&d->lock);
                


                    strncpy(child->name, name, MAXNAMLEN);


                    if(smartptr_is_null(child->sb))
                        child->sb = smartptr_ref(inode->sb);

                    child->parent = inode->parent;
                    break;
                }
                    
                
                
                i += d->rec_len;
            }

        });


        if(child != NULL)
            break;

    }

    return child;
}