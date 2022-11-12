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




void block_parse_partitions(device_t* device, inode_t* inode, void (*device_mkdev_fn) (device_t*, mode_t)) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device_mkdev_fn);
    DEBUG_ASSERT(inode);



    // * Check Partition Table EFI

    char efi[8];

    if(block_read(device, &efi, BLOCK_GPT_PARTITION_MAGIC_LBA * device->blk.blksize, sizeof(efi)) != sizeof(efi))
        kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(0)\n", BLOCK_GPT_PARTITION_MAGIC_LBA);
        

    if(strncmp(efi, BLOCK_GPT_PARTITION_MAGIC, sizeof(efi)) == 0) {

#if DEBUG_LEVEL_TRACE
        kprintf("device::block: Found EFI Partition Table in /dev/%s\n", device->name);
#endif


        // * Read GPT Header

        block_gpt_partition_t gpt = { 0 };

        if(block_read(device, &gpt, BLOCK_GPT_PARTITION_MAGIC_LBA * device->blk.blksize, sizeof(gpt)) != sizeof(gpt))
            kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(0)\n", BLOCK_GPT_PARTITION_MAGIC_LBA);
        


        // * Read GPT Entries

        for(size_t i = 0; i < gpt.table_entries; i++) {


            block_gpt_partition_entry_t entry = { 0 };

            if(block_read(device, &entry, (gpt.lba_table * device->blk.blksize) + (i * gpt.table_entry_size), sizeof(entry)) != sizeof(entry))
                kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(%ld)\n", gpt.lba_table * device->blk.blksize, i * gpt.table_entry_size);
        

            if(entry.type_guid[0] == 0)
                continue;

            if(entry.type_guid[0] == 0xFF)
                continue;



            device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);

            d->type = DEVICE_TYPE_BLOCK;
            
            strncpy(d->name, device->name, DEVICE_MAXNAMELEN);
            strncpy(d->description, device->description, DEVICE_MAXDESCLEN);


            if(i > 99) {

                d->name[strlen(device->name) + 0] = '1' + (i / 100);
                d->name[strlen(device->name) + 1] = '0' + (i % 100) / 10;
                d->name[strlen(device->name) + 2] = '0' + (i % 10);
                d->name[strlen(device->name) + 3] = '\0';

            } else if(i > 9) {

                d->name[strlen(device->name) + 0] = '1' + (i / 10);
                d->name[strlen(device->name) + 1] = '0' + (i % 10);
                d->name[strlen(device->name) + 2] = '\0';

            } else {

                d->name[strlen(device->name) + 0] = '1' + i;
                d->name[strlen(device->name) + 1] = '\0';

            }


            d->major = device->major;
            d->minor = device->minor + i + 1;

            d->init = NULL;
            d->dnit = NULL;
            d->reset = NULL;


            d->blk.blkmax   = device->blk.blkmax;
            d->blk.blksize  = device->blk.blksize;
            d->blk.blkoff   = entry.lba_first;
            d->blk.blkcount = entry.lba_last - entry.lba_first + 1;

            d->blk.read  = device->blk.read;
            d->blk.write = device->blk.write;

            d->userdata = device->userdata;



            mode_t mode;

            struct stat st;
            if(vfs_getattr(inode, &st) < 0) {
                mode = 0666;
            } else {
                mode = st.st_mode;
            }

            device_mkdev_fn(d, mode & 0777);

        }



    } else {

        // * Check Partition Table MBR

        uint16_t sig;

        if(block_read(device, &sig, BLOCK_DOS_PARTITION_MAGIC_OFFSET, sizeof(sig)) <= 0)
            kpanicf("device::block: ERROR! Read Error at offset lba(0) offset(0x1FE)\n");


        if(unlikely(sig != BLOCK_DOS_PARTITION_MAGIC)) {

    #if DEBUG_LEVEL_WARN
            kprintf("device::block: WARN! unknown partition table for /dev/%s (sig: %p)\n", device->name, sig);
    #endif

            return;

        }


#if DEBUG_LEVEL_TRACE
        kprintf("device::block: Found DOS Partition Table in /dev/%s\n", device->name);
#endif



        block_dos_partition_t part[4] = { 0 };

        if(block_read(device, &part, BLOCK_DOS_PARTITION_TABLE_OFFSET, sizeof(part)) != sizeof(part)) {
         
            kpanicf("device::block: ERROR! Read Error at offset lba(0) offset(0x1BE)\n");

        }


        for(size_t i = 0; i < BLOCK_DOS_PARTITION_TABLE_ENTRIES; i++) {

            if(part[i].type == BLOCK_DOS_PARTITION_TYPE_EMPTY)
                continue;

            
            device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);

            d->type = DEVICE_TYPE_BLOCK;
            
            strncpy(d->name, device->name, DEVICE_MAXNAMELEN);
            strncpy(d->description, device->description, DEVICE_MAXDESCLEN);


            d->name[strlen(device->name) + 0] = '1' + i;
            d->name[strlen(device->name) + 1] = '\0';


            d->major = device->major;
            d->minor = device->minor + i + 1;

            d->init = NULL;
            d->dnit = NULL;
            d->reset = NULL;


            d->blk.blkmax   = device->blk.blkmax;
            d->blk.blksize  = device->blk.blksize;
            d->blk.blkoff   = part[i].lba_start;
            d->blk.blkcount = part[i].lba_blocks;

            d->blk.read  = device->blk.read;
            d->blk.write = device->blk.write;

            d->userdata = device->userdata;



            mode_t mode;

            struct stat st;
            if(vfs_getattr(inode, &st) < 0) {
                mode = 0666;
            } else {
                mode = st.st_mode;
            }

            device_mkdev_fn(d, mode & 0777);

        }

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