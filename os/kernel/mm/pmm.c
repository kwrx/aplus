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
#include <aplus/ipc.h>
#include <aplus/mm.h>
#include <stdint.h>
#include <string.h>

static uint32_t* bitmap;
static uint32_t bitmap_size;
static spinlock_t bitmap_lock;

static void pmm_init_bitmap(uintptr_t start) {
    bitmap = (uint32_t*) start;
    bitmap_size = (mbd->memory.size / mbd->memory.pagesize) / 8;
    bitmap_size >>= 2;
    bitmap_size <<= 2;
    memset(bitmap, 0, bitmap_size);
}


void pmm_claim(uintptr_t start, uintptr_t end) {
    if((uint64_t) end > mbd->memory.size)
        return;


    start /= mbd->memory.pagesize;
    end /= mbd->memory.pagesize;


    __lock(&bitmap_lock, {

        mbd->memory.used += (end - start) * mbd->memory.pagesize;

        for(; start <= end; start++)
            bitmap[start / 32] |= (1 << (start % 32));

    });
}


void pmm_unclaim(uintptr_t start, uintptr_t end) {
    if((uint64_t) end > mbd->memory.size)
        return;


    start /= mbd->memory.pagesize;
    end /= mbd->memory.pagesize;

    __lock(&bitmap_lock, {

        mbd->memory.used += (end - start) * mbd->memory.pagesize;

        for(; start <= end; start++)
            bitmap[start / 32] &= ~(1 << (start % 32));

    });
}

block_t pmm_alloc_block(void) {
    DEBUG_ASSERT(sizeof(*bitmap) == 4);

    block_t block = -1;

    __lock(&bitmap_lock, {
        int i;
        for(i = 0; (i < (bitmap_size / 4)) && (block == -1); i++) {
            if(unlikely(bitmap[i] == 0xFFFFFFFF))
                continue;


            int m = bitmap[i];
            int j = 0;

            for(; j < 32; j++) {
                if(m & (1 << j))
                    continue;

                bitmap[i] |= (1 << j);
                block = (block_t) (i * 32) + j;
                break;
            }
        }
    });


    DEBUG_ASSERT(block != -1);

    mbd->memory.used += mbd->memory.pagesize;
    return block;
}


block_t pmm_alloc_block_from_end(void) {
    DEBUG_ASSERT(sizeof(*bitmap) == 4);

    block_t block = -1;

    __lock(&bitmap_lock, {
        int i;
        for(i = (bitmap_size / 4) - 1; i >= 0 && (block == -1); i--) {
            if(unlikely(bitmap[i] == 0xFFFFFFFF))
                continue;


            int m = bitmap[i];
            int j = 0;

            for(; j < 32; j++) {
                if(m & (1 << j))
                    continue;

                bitmap[i] |= (1 << j);
                block = (block_t) (i * 32) + j;
                break;
            }
        }
    });


    DEBUG_ASSERT(block != -1);

    mbd->memory.used += mbd->memory.pagesize;
    return block;
}


void pmm_free_block(block_t p) {
    DEBUG_ASSERT(sizeof(*bitmap) == 4);
    DEBUG_ASSERT(p != -1);
    DEBUG_ASSERT(p < (mbd->memory.size / mbd->memory.pagesize));


    __lock(&bitmap_lock, {
        bitmap[p / 32] &= ~(1 << (p % 32));
    });

    mbd->memory.used -= mbd->memory.pagesize;
}

void pmm_init(void) {
    spinlock_init(&bitmap_lock);
    pmm_init_bitmap(mbd->memory.start);
    
    
    int i;
    for(i = 0; i < mbd->mmap.count; i++) {
        if(mbd->mmap.ptr[i].type == MBD_MMAP_AVAILABLE)
            continue;

        if(mbd->mmap.ptr[i].address > mbd->memory.size)
            continue;

        if(mbd->mmap.ptr[i].address + mbd->mmap.ptr[i].length > mbd->memory.size)
            continue;

        pmm_claim (
            (uintptr_t) mbd->mmap.ptr[i].address,
            (uintptr_t) mbd->mmap.ptr[i].address + mbd->mmap.ptr[i].length
        );
    }

    pmm_claim(0, mbd->memory.start - CONFIG_KERNEL_BASE + bitmap_size);


    kprintf (
        "pmm: physical memory: %llu KB, used %llu KB, bitmap-size: %d KB\n",
        mbd->memory.size / 1024,
        mbd->memory.used / 1024,
        bitmap_size / 1024
    );
}