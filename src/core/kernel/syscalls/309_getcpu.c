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
#include <aplus/syscall.h>
#include <aplus/task.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>


/***
 * Name:        getcpu
 * Description: determine CPU and NUMA node on which the calling thread is
       running
 * URL:         http://man7.org/linux/man-pages/man2/getcpu.2.html
 *
 * Input Parameters:
 *  0: 0x135
 *  1: unsigned __user * cpu
 *  2: unsigned __user * node
 *  3: struct getcpu_cache __user * cache
 *
 * Auto-generated by extra/utils/gen-syscalls.js
 */

struct getcpu_cache;

SYSCALL(309, getcpu,
long sys_getcpu (unsigned __user * cpu, unsigned __user * node, struct getcpu_cache __user * cache) {
    return -ENOSYS;
    // DEBUG_ASSERT(cache == NULL);


    // if(likely(cpu)) {

    //     if(unlikely(!uio_check(cpu, R_OK | W_OK)))
    //         return -EFAULT;

    //     *(cpu) = current_cpu->id;

    // }
        

    // if(likely(node)) {

    //     if(unlikely(!uio_check(node, R_OK | W_OK)))
    //         return -EFAULT;

    //     *(node) = current_cpu->node;
    
    // }

    // return 0;
});
