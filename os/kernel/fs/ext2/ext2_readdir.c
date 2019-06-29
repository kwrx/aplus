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
int ext2_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->fsinfo);
    DEBUG_ASSERT(e);


    ext2_t* ext2 = (ext2_t*) inode->fsinfo;
    uint32_t off = 0;



    struct ext2_inode i;
    ext2_utils_read_inode(ext2, inode->st.st_ino, &i);



    int q;
    for(q = 0; q < pos; q++) {

        if(off >= i.i_size)
            return 0;


        struct ext2_dir_entry_2 d;
        ext2_utils_read_inode_data(ext2, i.i_block, (off / ext2->blocksize), 
                                                    (off % ext2->blocksize), &d, sizeof(d));                          

        off += d.rec_len;                                            
    }



    for(q = 0; q < count; q++) {

        if(off >= i.i_size)
            break;


        struct ext2_dir_entry_2 d;
        ext2_utils_read_inode_data(ext2, i.i_block, (off / ext2->blocksize), 
                                                    (off % ext2->blocksize), &d, sizeof(d));            

        ext2_utils_read_inode_data(ext2, i.i_block, (off / ext2->blocksize), 
                                                    (off % ext2->blocksize) + 8, e->d_name, d.name_len);


        e->d_ino = d.inode;
        e->d_off = sizeof(struct dirent) * (pos + q);
        e->d_reclen = sizeof(struct dirent);
        e->d_type = 0;
        e->d_name[d.name_len] = '\0';


        if(ext2->sb.s_rev_level == EXT2_DYNAMIC_REV)
            e->d_type = d.file_type;


        off += d.rec_len;
    }


    return q;
}