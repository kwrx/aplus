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
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/ipc.h>
#include <aplus/timer.h>
#include <aplus/debug.h>
#include <aplus/syscall.h>
#include <libc.h>



static int inode_fifo_ioctl(struct inode* inode, int req, void* data) {
    if(unlikely(!inode)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata))
        return -1;

    fifo_t* fifo = (fifo_t*) inode->userdata;


    switch(req) {
        case FIONREAD:
            *(int*) data = fifo_available(fifo);
            break;

        case FIONBIO:
            fifo->nbio = !!(*(int*) data);
            break;

        case F_SETPIPE_SZ:
            *(size_t*) data = *(size_t*) data == 0
                                ? BUFSIZ
                                : (
                                    *(size_t*) data > CONFIG_IPC_PIPEMAX
                                        ? CONFIG_IPC_PIPEMAX
                                        : *(size_t*) data
                                );
            
            fifo_init(fifo, *(size_t*) data, fifo->nbio);
            break;
            
        case F_GETPIPE_SZ:
            *(size_t*) data = fifo->size;
            break;

        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}


static int inode_fifo_read(struct inode* inode, void* ptr, off_t pos, size_t len) {
    (void) pos;
    
    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata))
        return -1;


    fifo_t* fifo = (fifo_t*) inode->userdata;
    return fifo_read(fifo, ptr, len);        
}

static int inode_fifo_write(struct inode* inode, void* ptr, off_t pos, size_t len) {
    (void) pos;

    if(unlikely(!inode || !ptr)) {
        errno = EINVAL;
        return -1;
    }
    
    if(unlikely(!inode->userdata))
        return -1;
        
        
    fifo_t* fifo = (fifo_t*) inode->userdata;
    return fifo_write(fifo, ptr, len);
}


SYSCALL(30, mkfifo,
int sys_mkfifo(const char* pathname, mode_t mode) {
    int fd = sys_open(pathname, O_RDWR | O_CREAT | O_EXCL, S_IFIFO | mode);
    if(unlikely(fd < 0))
        return -1;
        
    
    fifo_t* fifo = (fifo_t*) kmalloc(sizeof(fifo_t), GFP_USER);
    if(unlikely(!fifo)) {
        errno = ENOMEM;
        return -1;
    }
    

    fifo_init(fifo, PAGE_SIZE, 0);
    
#if CONFIG_IPC_DEBUG
    #define mtxname strdup(pathname)
#else
    #define mtxname NULL
#endif
    
    mutex_init(&fifo->r_lock, MTX_KIND_DEFAULT, mtxname);
    mutex_init(&fifo->w_lock, MTX_KIND_DEFAULT, mtxname);
    
        
    inode_t* inode = current_task->fd[fd].inode;
    inode->read = inode_fifo_read;
    inode->write = inode_fifo_write;
    inode->ioctl = inode_fifo_ioctl;
    inode->userdata = (void*) fifo;
    
    sys_close(fd);
    return 0;
});
