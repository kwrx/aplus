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

#include "iso9660.h"





int iso9660_open(struct inode* inode) {
    if(unlikely(!inode)) {
        errno = EINVAL;
        return -1;
    }

    if(unlikely(!inode->userdata)) {
        errno = EINVAL;
        return -1;
    }
    

    iso9660_t* ctx = (iso9660_t*) inode->userdata;
    KASSERT(ctx);


    iso9660_dir_t* nodes = (iso9660_dir_t*) kmalloc(iso9660_getlsb32(ctx->dir.length), GFP_USER);
    iso9660_dir_t* snodes = nodes;        
    KASSERT(nodes);
    


    if(unlikely(vfs_read(ctx->dev, nodes, iso9660_getlsb32(ctx->dir.lba) * ISO9660_SECTOR_SIZE, iso9660_getlsb32(ctx->dir.length)) != iso9660_getlsb32(ctx->dir.length))) {
        kfree(nodes);

        errno = EIO;
        return -1;
    }


    
    /* Skip dots (".", "..") */
    nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size);
    nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size);

    int i;
    for(i = 0; i < iso9660_getlsb32(ctx->dir.length); i += ISO9660_SECTOR_SIZE) {
        if(i != 0)
            nodes = (iso9660_dir_t*) ((uintptr_t) snodes + i);
        
        for(
            ; 
            nodes->size;
            nodes = (iso9660_dir_t*) ((uintptr_t) nodes + nodes->size + nodes->exattr)
        ) {

            inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_ATOMIC);
            if(unlikely(!child)) {
                errno = ENOMEM;
                return -1;
            }

            memset(child, 0, sizeof(inode_t));
            
            
#if HAVE_ROCKRIDGE
            register int len = nodes->idlen;
            if(!(len & 1))
                len++;
            
            void* rockridge_offset = (void*) ((uintptr_t) &nodes->reserved + len);
#endif    


#if HAVE_ROCKRIDGE            
            child->name = rockridge_getname(rockridge_offset);
#else

            child->name = (const char*) kmalloc(nodes->idlen + 1, GFP_USER);
            KASSERT(child->name);
            
            memset((void*) child->name, 0, nodes->idlen + 1);
            strncpy((char*) child->name, nodes->reserved, nodes->idlen);
            
            iso9660_checkname((char*) child->name);
#endif

        
            struct inode_childs* cx;
            for(cx = inode->childs; cx; cx = cx->next) {
                if(strcmp(cx->inode->name, child->name) == 0) {
                    kfree((void*) child->name);
                    kfree((void*) child);
                    
                    child = NULL;
                    break;
                }
            }

            if(unlikely(!child))
                continue;

            
            child->ino = vfs_inode();
            child->mode = (nodes->flags & ISO9660_FLAGS_DIRECTORY ? S_IFDIR : S_IFREG) | 0666 & ~current_task->umask;

            child->dev =
            child->rdev =
            child->nlink = 0;

            child->uid = current_task->uid;
            child->gid = current_task->gid;
            child->size = (off64_t) iso9660_getlsb32(nodes->length);

            child->atime = 
            child->ctime = 
            child->mtime = timer_gettimestamp();
        
            child->parent = inode;
            child->link = NULL;

            child->childs = NULL;


            if(nodes->flags & ISO9660_FLAGS_DIRECTORY) {
                child->open = iso9660_open;
                child->close = iso9660_close;
                child->finddir = iso9660_finddir;
                child->unlink = iso9660_unlink;
            } else {
                child->read = iso9660_read;
                child->write = iso9660_write;
            }
            
            child->chown = NULL;
            child->chmod = NULL;
            child->ioctl = NULL;
            
#if HAVE_ROCKRIDGE
            rockridge_getmode(rockridge_offset, &child->mode,
                                                &child->uid,
                                                &child->gid,
                                                &child->nlink);
#endif
            

            iso9660_t* cctx = (iso9660_t*) kmalloc(sizeof(iso9660_t), GFP_USER);
            KASSERT(cctx);

            memcpy(cctx, ctx, sizeof(iso9660_t));
            memcpy(&cctx->dir, nodes, sizeof(iso9660_dir_t));

            child->userdata = (void*) cctx;
            child->mtinfo = inode->mtinfo;



            

            cx = (struct inode_childs*) kmalloc(sizeof(struct inode_childs), GFP_KERNEL);
            cx->inode = child;
            cx->next = inode->childs;
            inode->childs = cx;
        }
    }

    kfree(snodes);
    return 0;
}