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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/memory.h>
#include <aplus/smp.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <stdio.h>

#include <dev/interface.h>
#include <dev/block.h>


MODULE_NAME("dev/block");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



#define __cache_is_cached(x, y)                 \
    (x.c_cached && (x.c_blkno == y))

#define __cache_update(x, y, z)                 \
    {                                           \
        x.c_blkno = y;                          \
        x.c_cached = z;                         \
    }



ssize_t block_write(device_t* device, const void* buf, off_t offset, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(device->blk.blksize < sizeof(device->blk.cache.c_data));
    DEBUG_ASSERT(buf);


    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    if(!device->blk.write || !device->blk.read)
        return 0;

    if(unlikely(!size))
        return 0;

    if(offset >= (device->blk.blkcount * device->blk.blksize))
        return 0;

    if(offset + size >= (device->blk.blkcount * device->blk.blksize))
        size = (device->blk.blkcount * device->blk.blksize) - offset;

    if(unlikely(!size))
        return 0;


    current_task->rusage.ru_oublock++; 




    offset += device->blk.blkoff * device->blk.blksize;

    long sb = offset / device->blk.blksize;   
    long eb = (offset + size - 1) / device->blk.blksize;  

    off_t xoff = 0;


    if(offset % device->blk.blksize) {
        
        long p;
        p = device->blk.blksize - (offset % device->blk.blksize);
        p = p > size ? size : p;


        if(unlikely(!__cache_is_cached(device->blk.cache, sb))) {
            
            if(device->blk.read(device, device->blk.cache.c_data, sb, 1) <= 0)
                return errno = EIO, xoff;

            __cache_update(device->blk.cache, sb, 1);
        
        }

        memcpy((void*) ((uintptr_t) device->blk.cache.c_data + ((uintptr_t) offset % device->blk.blksize)), buf, p);

        if(device->blk.write(device, device->blk.cache.c_data, sb, 1) <= 0)
            return errno = EIO, xoff;

        xoff += p;
        sb++;

    }



    if(((offset + size) % device->blk.blksize) && (sb <= eb)) {
        
        long p;
        p = (offset + size) % device->blk.blksize;


        if(unlikely(!__cache_is_cached(device->blk.cache, eb))) {

            if(device->blk.read(device, device->blk.cache.c_data, eb, 1) <= 0)
                return errno = EIO, xoff;

            __cache_update(device->blk.cache, eb, 1);

        }

        memcpy(device->blk.cache.c_data, (void*) ((uintptr_t) buf + size - p), p);
       
        if(device->blk.write(device, device->blk.cache.c_data, eb, 1) <= 0)
            return errno = EIO, xoff;
       
        eb--;

    }



    long i = eb - sb + 1;

    if(likely(i > 0)) {

        if(device->blk.blkmax) {
         
            long max = (long) device->blk.blkmax;

            for (;
                i - max >= 0;
                i -= max,
                sb += max,
                xoff += device->blk.blksize * max
            ) {

                if(unlikely(device->blk.write(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, device->blk.blkmax)) <= 0)
                    return errno = EIO, xoff;
            
            }

        }        


        if(likely(i > 0)) {

            if(unlikely(device->blk.write(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, i)) <= 0)
                return errno = EIO, xoff;

        }

    
    }

    return size;
}



ssize_t block_read(device_t* device, void* buf, off_t offset, size_t size) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(device->blk.blksize < sizeof(device->blk.cache.c_data));
    DEBUG_ASSERT(buf);


    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    if(!device->blk.read)
        return 0;

    if(unlikely(!size))
        return 0;

    if(offset >= (device->blk.blkcount * device->blk.blksize))
        return 0;

    if(offset + size >= (device->blk.blkcount * device->blk.blksize))
        size = (device->blk.blkcount * device->blk.blksize) - offset;

    if(unlikely(!size))
        return 0;


    current_task->rusage.ru_inblock++; 



    offset += device->blk.blkoff * device->blk.blksize;

    long sb = (offset) / device->blk.blksize;   
    long eb = (offset + size - 1) / device->blk.blksize;   

    off_t xoff = 0;


    if(offset % device->blk.blksize) {
        
        long p;
        p = device->blk.blksize - (offset % device->blk.blksize);
        p = p > size ? size : p;


        if(unlikely(!__cache_is_cached(device->blk.cache, sb))) {
            
            if(device->blk.read(device, device->blk.cache.c_data, sb, 1) <= 0)
                return errno = EIO, xoff;

            __cache_update(device->blk.cache, sb, 1);
        
        }

        memcpy(buf, (void*) ((uintptr_t) device->blk.cache.c_data + ((uintptr_t) offset % device->blk.blksize)), p);

        xoff += p;
        sb++;

    }


    if(((offset + size) % device->blk.blksize) && (sb <= eb)) {
        
        long p;
        p = (offset + size) % device->blk.blksize;


        if(unlikely(!__cache_is_cached(device->blk.cache, eb))) {

            if(device->blk.read(device, device->blk.cache.c_data, eb, 1) <= 0)
                return errno = EIO, xoff;

            __cache_update(device->blk.cache, eb, 1);

        }

        memcpy((void*) ((uintptr_t) buf + size - p), device->blk.cache.c_data, p);
        eb--;

    }


    long i = eb - sb + 1;
    
    if(likely(i > 0)) {
       
        if(device->blk.blkmax) {
         
            long max = (long) device->blk.blkmax;

            for (;
                i - max >= 0;
                i -= max,
                sb += max,
                xoff += device->blk.blksize * max
            ) {

                if(unlikely(device->blk.read(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, device->blk.blkmax)) <= 0)
                    return errno = EIO, xoff;

            }

        }        


        if(likely(i > 0)) {

            if(unlikely(device->blk.read(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, i)) <= 0)
                return errno = EIO, xoff;

        }

    }

    return size;
    
}



void block_init(device_t* device) {
    DEBUG_ASSERT(device);

    device->blk.cache.c_cached = 0;
}

void block_dnit(device_t* device) {
    DEBUG_ASSERT(device);

    device->blk.cache.c_cached = 0;
}



void init(const char* args) {
    (void) args;
}

void dnit(void) {
    
}