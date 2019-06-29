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

#include <aplus/utils/list.h>

#include "ext2.h"


__thread_safe
inode_t* ext2_finddir(inode_t* inode, const char * name) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->fsinfo);
    DEBUG_ASSERT(name);


    ext2_t* ext2 = (ext2_t*) inode->fsinfo;



    struct ext2_inode e;
    ext2_utils_read_inode(ext2, inode->st.st_ino, &e);



    char entry[MAXNAMLEN];

    int q;
    for(q = 0; q < e.i_size; q++) {

        struct ext2_dir_entry_2 d;
        ext2_utils_read_inode_data(ext2, e.i_block, (q / ext2->blocksize), 
                                                    (q % ext2->blocksize), &d, sizeof(d));

        ext2_utils_read_inode_data(ext2, e.i_block, (q / ext2->blocksize), 
                                                    (q % ext2->blocksize) + 8, entry, d.name_len);

        entry[d.name_len] = '\0';



        if(strcmp(entry, name) == 0) {
            
            inode_t* i = (inode_t*) kcalloc(1, sizeof(inode_t), GFP_USER);  /* Add Caching */

            i->parent = inode;
            i->st.st_ino = d.inode;
            i->st.st_mode = e.i_mode;
            i->st.st_size = e.i_size;
            i->st.st_nlink = e.i_links_count;
            i->st.st_atim.tv_sec = e.i_atime;
            i->st.st_ctim.tv_sec = e.i_ctime;
            i->st.st_mtim.tv_sec = e.i_mtime;
            i->st.st_uid = e.i_uid;
            i->st.st_gid = e.i_gid;

            strncpy(i->name, name, sizeof(i->name));


            
            if((inode->st.st_mode & S_IFMT) == S_IFMT)
                i->root = inode;
            else
                i->root = inode->root;

            DEBUG_ASSERT(i->root);


            if(S_ISDIR(mode)) {

                i->ops.finddir = tmpfs_finddir;
                i->ops.readdir = tmpfs_readdir;
                i->ops.unlink = tmpfs_unlink;
                i->ops.mknod = tmpfs_mknod;

            }

            if(S_ISREG(mode)) {

                i->ops.read = tmpfs_read;     
                i->ops.write = tmpfs_write;     

            }

            spinlock_init(&i->lock);


            i->root->mount.st.f_ffree--;
            i->root->mount.st.f_favail--;

            i->fsinfo = i->root->fsinfo;

            
        }

        q += d.rec_len;                                            
    }


    return NULL;
}