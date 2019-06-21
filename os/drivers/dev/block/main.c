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
#include <aplus/module.h>
#include <aplus/mm.h>
#include <aplus/smp.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

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



__thread_safe
int block_write(device_t* device, const void* buf, off_t offset, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(device->blk.blksize < sizeof(device->blk.cache.c_data));
    DEBUG_ASSERT(buf);


    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    offset += device->blk.blkoff * device->blk.blksize;



    if(!device->blk.write || !device->blk.read)
        return 0;

    if(unlikely(!size))
        return 0;

    if(offset >= (device->blk.blkcount * device->blk.blksize))
        return 0;

    if(offset + size >= (device->blk.blkcount * device->blk.blksize))
        size = (device->blk.blkcount * device->blk.blksize) - offset;

    DEBUG_ASSERT(size >= 0);

    if(unlikely(!size))
        return 0;


    current_task->rusage.ru_oublock++; 


    uint32_t sb = offset / device->blk.blksize;   
    uint32_t eb = (offset + size - 1) / device->blk.blksize;   
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
         
            for(; sb + device->blk.blkmax < i; sb += device->blk.blkmax, xoff += device->blk.blksize * device->blk.blkmax)
                if(unlikely(device->blk.write(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, device->blk.blkmax)) <= 0)
                    return errno = EIO, xoff;
        
            if(unlikely(device->blk.write(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, i - sb)) <= 0)
                return errno = EIO, xoff;

        } else {

            if(unlikely(device->blk.write(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, i)) <= 0)
                return errno = EIO, xoff;

        }
    
    }

    return size;
}


__thread_safe
int block_read(device_t* device, void* buf, off_t offset, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(device->blk.blksize < sizeof(device->blk.cache.c_data));
    DEBUG_ASSERT(buf);


    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


   
    offset += device->blk.blkoff * device->blk.blksize;



    if(!device->blk.read)
        return 0;

    if(unlikely(!size))
        return 0;

    if(offset >= (device->blk.blkcount * device->blk.blksize))
        return 0;

    if(offset + size >= (device->blk.blkcount * device->blk.blksize))
        size = (device->blk.blkcount * device->blk.blksize) - offset;

    DEBUG_ASSERT(size >= 0);

    if(unlikely(!size))
        return 0;


    current_task->rusage.ru_inblock++; 


    uint32_t sb = offset / device->blk.blksize;   
    uint32_t eb = (offset + size - 1) / device->blk.blksize;   
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
         
            int max = (int) device->blk.blkmax;

            for (;
                i - max >= 0;
                i -= max,
                sb += max,
                xoff += device->blk.blksize * max
            )
                if(unlikely(device->blk.read(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, device->blk.blkmax)) <= 0)
                    return errno = EIO, xoff;

        }        


        if(likely(i > 0))
            if(unlikely(device->blk.read(device, (void*) ((uintptr_t) buf + (uintptr_t) xoff), sb, i)) <= 0)
                return errno = EIO, xoff;

    }

    return size;
}




void block_inode(device_t* device, inode_t* inode, void (*device_mkdev) (device_t*, mode_t)) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device_mkdev);
    DEBUG_ASSERT(inode);

    inode->st.st_size = device->blk.blkcount *
                        device->blk.blksize  ;

    inode->st.st_blksize = device->blk.blksize;



    /* Check Partition Table */

    char efi[8];

    if(block_read(device, &efi, device->blk.blksize * 1, 8) <= 0) {
        kprintf("device::block: ERROR! Read Error at lba(1) offset(0)");
        return;
    }

    if(strncmp(efi, "EFI PART", 8) == 0)
        kpanic("device::block: unsupported partition type GPT for /dev/%s", device->name);



    struct {
        uint8_t flags;
        uint8_t h;
        uint16_t s:6;
        uint16_t c:10;
        uint8_t id;
        uint8_t eh;
        uint16_t es:6;
        uint16_t ec:10;
        uint32_t lba;
        uint32_t size;
    } __packed mbr[4] = { };

    if(block_read(device, &mbr, 0x1BE, sizeof(mbr[0]) * 4) <= 0)
        kprintf("device::block: ERROR! Read Error at offset lba(0) offset(0x1BE)");



    int i;
    for(i = 0; i < 4; i++) {

        if(mbr[i].size == 0)
            continue;

        
        device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);

        d->type = DEVICE_TYPE_BLOCK;
        
        strncpy(d->name, device->name, DEVICE_MAXNAMELEN);
        strncpy(d->description, device->description, DEVICE_MAXDESCLEN);

        d->name[strlen(device->name)] += '1' + i;
        d->name[strlen(device->name) + 1] += '\0';

        d->major = device->major;
        d->minor = device->minor + i + 1;

        d->init = NULL;
        d->dnit = NULL;
        d->reset = NULL;


        d->blk.blkmax = device->blk.blkmax;
        d->blk.blksize = device->blk.blksize;
        d->blk.blkoff = mbr[i].lba;
        d->blk.blkcount = d->blk.blkoff + mbr[i].size;

        d->blk.read = device->blk.read;
        d->blk.write = device->blk.write;

        d->userdata = device->userdata;


        device_mkdev(d, inode->st.st_mode & 0777);

    }

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