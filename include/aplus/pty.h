/*                                                                      
 * Author(s):                                                           
 *      Antonino Natale <antonio.natale97@hotmail.com>                  
 *                                                                      
 * Copyright (c) 2013-2019 Antonino Natale                              
 *                                                                      
 *                                                                      
 * This file is part of aplus.                                          
 *                                                                      
 * aplus is free software: you can redistribute it and/or modify        
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or    
 * (at your option) any later version.                                  
 *                                                                      
 * aplus is distributed in the hope that it will be useful,             
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        
 * GNU General Public License for more details.                         
 *                                                                      
 * You should have received a copy of the GNU General Public License    
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.       
 */                                                                     
                                                                        
#ifndef _APLUS_PTY_H
#define _APLUS_PTY_H

#ifndef __ASSEMBLY__



#if defined(KERNEL) || defined(MODULE)

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/hal.h>
#include <aplus/errno.h>
#include <aplus/utils/ringbuffer.h>

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>


typedef struct pty {

    uint64_t index;

    struct winsize ws;
    struct termios ios;

    ringbuffer_t r1;
    ringbuffer_t r2;

    pid_t m_sid;
    pid_t m_pid;
    pid_t s_pgrp;

    struct {
        char* buffer;
        size_t size;
        size_t capacity;
        spinlock_t lock;
    } input;

    bool locked;

    inode_t* ptmx;

    struct pty* next;

} pty_t;


__BEGIN_DECLS

int pty_ioctl(inode_t* inode, long req, void* arg);

ssize_t pty_master_read(inode_t* inode, void* buf, off_t offset, size_t size);
ssize_t pty_master_write(inode_t* inode, const void* buf, off_t offset, size_t size);
ssize_t pty_slave_read(inode_t* inode, void* buf, off_t offset, size_t size);
ssize_t pty_slave_write(inode_t* inode, const void* buf, off_t offset, size_t size);

pty_t* pty_create(inode_t* ptmx, int flags);
pty_t* pty_queue();
void pty_queue_lock();
void pty_queue_unlock();

__END_DECLS

#endif

#endif
#endif
