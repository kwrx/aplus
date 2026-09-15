/*
 * Author:
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



#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/hal.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "procfs.h"


static int procfs_service_meminfo_fetch(inode_t* inode, char** buf, size_t* size, void* arg) {

    (void)arg;

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->sb);
    DEBUG_ASSERT(inode->sb->fsid == FSID_PROCFS);

    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    procfs_buf_t b = procfs_scratch();

    //? Sampled once each: pmm_get_used_memory() sums the whole page-usage table under a
    //? lock, and this used to call it three times per read.
    uint64_t total = pmm_get_total_memory();
    uint64_t used  = pmm_get_used_memory();
    uint64_t slab  = kheap_get_used_memory();

    uint64_t free = (total > used) ? (total - used) : 0;

    /* Every key carries its colon. Active(anon), Inactive(anon), Active(file) and
       Inactive(file) were written without one, which breaks any reader splitting on ":"
       -- and free(1) is exactly such a reader. */
    procfs_bprintf(&b, "MemTotal:       %lu kB\n", total >> 10);
    procfs_bprintf(&b, "MemFree:        %lu kB\n", free >> 10);

    //? Identical to MemFree by definition here: there is no reclaimable page cache to add.
    procfs_bprintf(&b, "MemAvailable:   %lu kB\n", free >> 10);

    procfs_bprintf(&b, "Buffers:        0 kB\n");
    procfs_bprintf(&b, "Cached:         0 kB\n");
    procfs_bprintf(&b, "SwapCached:     0 kB\n");
    procfs_bprintf(&b, "Active:         0 kB\n");
    procfs_bprintf(&b, "Inactive:       0 kB\n");
    procfs_bprintf(&b, "Active(anon):   0 kB\n");
    procfs_bprintf(&b, "Inactive(anon): 0 kB\n");
    procfs_bprintf(&b, "Active(file):   0 kB\n");
    procfs_bprintf(&b, "Inactive(file): 0 kB\n");

    //? No swap support, so these are structurally zero rather than unknown.
    procfs_bprintf(&b, "SwapTotal:      0 kB\n");
    procfs_bprintf(&b, "SwapFree:       0 kB\n");

    procfs_bprintf(&b, "Dirty:          0 kB\n");
    procfs_bprintf(&b, "Writeback:      0 kB\n");
    procfs_bprintf(&b, "AnonPages:      0 kB\n");
    procfs_bprintf(&b, "Mapped:         0 kB\n");
    procfs_bprintf(&b, "Shmem:          0 kB\n");
    procfs_bprintf(&b, "Slab:           %lu kB\n", slab >> 10);
    procfs_bprintf(&b, "SReclaimable:   0 kB\n");
    procfs_bprintf(&b, "SUnreclaim:     %lu kB\n", slab >> 10);
    procfs_bprintf(&b, "KernelStack:    0 kB\n");
    procfs_bprintf(&b, "PageTables:     0 kB\n");

    *buf  = b.data;
    *size = b.length;

    return 0;
}

inode_t* procfs_service_meminfo_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode      = procfs_service_inode(parent, "meminfo", S_IFREG | 0444, procfs_service_meminfo_fetch, NULL);
        inode->ino = PROCFS_INO_STATIC(2);
    }

    return inode;
}
