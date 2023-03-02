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
#include <string.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/ipc.h>
#include <aplus/smp.h>
#include <aplus/hal.h>
#include <aplus/errno.h>

#include <dev/interface.h>
#include <dev/block.h>
#include <dev/pci.h>

#include <arch/x86/cpu.h>


MODULE_NAME("block/ram");
MODULE_DEPS("dev/interface,dev/block");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#define RAMDISK_BLKSIZE             4096
#define RAMDISK_BLK2BYTES(blk)      ((blk) * RAMDISK_BLKSIZE)
#define RAMDISK_BYTES2BLK(blk)      ((blk) / RAMDISK_BLKSIZE)


static ssize_t ram_read(device_t* device, void* buf, off_t offset, size_t count) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);


    uintptr_t index = (uintptr_t) device->userdata;


    if(unlikely(index >= CORE_MODULE_MAX))
        return errno = EIO, -1;

    if(unlikely(core->modules.ko[index].status != MODULE_STATUS_LOADED))
        return errno = EIO, -1;



    uintptr_t ptr  = (uintptr_t) core->modules.ko[index].ptr;
    uintptr_t size = (uintptr_t) core->modules.ko[index].size;

        
    if(unlikely(RAMDISK_BLK2BYTES(offset) >= size))
        return 0;

    if(unlikely(RAMDISK_BLK2BYTES(offset + count) > size))
        count = RAMDISK_BYTES2BLK(size - RAMDISK_BLK2BYTES(offset));
    
    if(unlikely(count == 0))
        return 0;


    memcpy (
        buf,
        (void*) (arch_vmm_p2v(ptr, ARCH_VMM_AREA_HEAP) + RAMDISK_BLK2BYTES(offset)),
        RAMDISK_BLK2BYTES(count)
    );
    

    return count;

}


static ssize_t ram_write(device_t* device, const void* buf, off_t offset, size_t count) {
    
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(count);


    uintptr_t index = (uintptr_t) device->userdata;


    if(unlikely(index >= CORE_MODULE_MAX))
        return errno = EIO, -1;

    if(unlikely(core->modules.ko[index].status != MODULE_STATUS_LOADED))
        return errno = EIO, -1;



    uintptr_t ptr  = (uintptr_t) core->modules.ko[index].ptr;
    uintptr_t size = (uintptr_t) core->modules.ko[index].size;

        
    if(unlikely(RAMDISK_BLK2BYTES(offset) >= size))
        return 0;

    if(unlikely(RAMDISK_BLK2BYTES(offset + count) > size))
        count = RAMDISK_BYTES2BLK(size - RAMDISK_BLK2BYTES(offset));
    
    if(unlikely(count == 0))
        return 0;


    memcpy (
        (void*) (arch_vmm_p2v(ptr, ARCH_VMM_AREA_HEAP) + RAMDISK_BLK2BYTES(offset)),
        buf,
        RAMDISK_BLK2BYTES(count)
    );
    

    return count;
    
}




void init(const char* args) {


    DEBUG_ASSERT(core);
    DEBUG_ASSERT(core->modules.count > 0);


    uintptr_t index;
    uintptr_t device;

    for(index = device = 0; index < core->modules.count; index++) {

        if(core->modules.ko[index].status == MODULE_STATUS_LOADED)
            continue;

        if(core->modules.ko[index].ptr == 0)
            continue;

        if(core->modules.ko[index].size == 0)
            continue;

        if(strstr((const void*) core->modules.ko[index].cmdline, "type=ramdisk") == NULL)
            continue;



        device_t* d = (device_t*) kcalloc(sizeof(device_t), 1, GFP_KERNEL);

        strncpy(d->name, "ram0", DEVICE_MAXNAMELEN);
        strncpy(d->description, "RAM disk device", DEVICE_MAXDESCLEN);

        d->name[3] += device;
        d->type     = DEVICE_TYPE_BLOCK;
        d->major    = device ? 65 + device : 8;
        d->minor    = device << 3;


        d->init    = NULL;
        d->dnit    = NULL;
        d->reset   = NULL;


        d->blk.blksize  = RAMDISK_BLKSIZE;
        d->blk.blkcount = RAMDISK_BYTES2BLK(core->modules.ko[index].size);
        d->blk.blkmax   = RAMDISK_BLKSIZE * 1024;
        d->blk.blkoff   = 0;

        d->blk.read  = ram_read;
        d->blk.write = ram_write;

        d->userdata = (void*) index;

        device += 1;


        core->modules.ko[index].status = MODULE_STATUS_LOADED;

#if DEBUG_LEVEL_INFO
        kprintf("block/ram: added %s (%d blocks, %d bytes)\n", d->name, d->blk.blkcount, RAMDISK_BLK2BYTES(d->blk.blkcount));
#endif

        device_mkdev(d, 0660);

    }

}



void dnit(void) {
    
}