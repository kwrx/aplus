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
#include <aplus/smp.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/utils/list.h>

#include <stdint.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>

#include <dev/interface.h>
#include <dev/char.h>
#include <dev/block.h>
#include <dev/video.h>



MODULE_NAME("dev/interface");
MODULE_DEPS("dev/char,dev/block,dev/video");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



static list(device_t*, devices);



static ssize_t device_read(inode_t* inode, void* buf, off_t off, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);



    device_t* device = (device_t*) inode->userdata;

    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    switch(device->type) {
        
        case DEVICE_TYPE_CHAR:
            return char_read(device, buf, size);

        case DEVICE_TYPE_BLOCK:
            return block_read(device, buf, off, size);

        case DEVICE_TYPE_VIDEO:
            return errno = ENOSYS, -1;

    }


    kpanicf("device::read: unknown device type %d for /dev/%s\n", device->type, inode->name);
}



static ssize_t device_write(inode_t* inode, const void* buf, off_t off, size_t size) {
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    device_t* device = (device_t*) inode->userdata;

    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    switch(device->type) {
        
        case DEVICE_TYPE_CHAR:
            return char_write(device, buf, size);

        case DEVICE_TYPE_BLOCK:
            return block_write(device, buf, off, size);

        case DEVICE_TYPE_VIDEO:
            return errno = ENOSYS, -1;

    }


    kpanicf("device::write: unknown device type %d for /dev/%s\n", device->type, inode->name);

}



static int device_ioctl(inode_t* inode, long req, void* arg) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(arg);


    device_t* device = (device_t*) inode->userdata;

    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    switch(device->type) {
        
        case DEVICE_TYPE_CHAR:
            return errno = ENOSYS, -1; /* TODO */

        case DEVICE_TYPE_BLOCK:
            return errno = ENOSYS, -1; /* TODO */

        case DEVICE_TYPE_VIDEO:
            return video_ioctl(device, req, arg);
    }


    kpanicf("device::ioctl: unknown device type %d for /dev/%s\n", device->type, inode->name);
}



static int device_fsync(inode_t* inode, int datasync) {

    DEBUG_ASSERT(inode);


    device_t* device = (device_t*) inode->userdata;

    if(device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    switch(device->type) {
        
        case DEVICE_TYPE_CHAR:
            return char_flush(device);

        case DEVICE_TYPE_BLOCK:
            return 0;

        case DEVICE_TYPE_VIDEO:
            return errno = ENOSYS, -1;

    }


    kpanicf("device::fsync: unknown device type %d for /dev/%s\n", device->type, inode->name);
}





void device_mkdev(device_t* device, mode_t mode) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->name);


    device->status = DEVICE_STATUS_LOADING;


    spinlock_init(&device->lock);

    switch(device->type) {
        case DEVICE_TYPE_CHAR:

            char_init(device);            
            break;

        case DEVICE_TYPE_BLOCK:

            block_init(device);
            break;

        case DEVICE_TYPE_VIDEO:

            video_init(device);
            break;

        default:
            kpanicf("device::create: failed, unknown device %s type %d\n", device->name, device->type);
    }


    if(likely(device->init))
        device->init(device);

    if(unlikely(device->status == DEVICE_STATUS_FAILED))
        kpanicf("device::create: fail on loading %s\n", device->name);

    device->status = DEVICE_STATUS_READY;



    char buf[MAXNAMLEN] = { 0 };
    strcpy(buf, "/dev/");
    strcat(buf, device->name);


    int fd = sys_creat(buf, mode | (device->type == DEVICE_TYPE_BLOCK ? S_IFBLK : S_IFCHR));

    if(unlikely(fd < 0))
        kpanicf("device::create: failed, device already exists: %s\n", buf);

    
    inode_t* i = current_task->fd[fd].inode;

    DEBUG_ASSERT(i);


    if(unlikely(sys_close(fd) < 0))
        kpanicf("device::create: failed, could not close device descriptor: %s\n", buf);


       
    i->userdata = (void*) device;

    i->ops.write = device_write;
    i->ops.read  = device_read;
    i->ops.ioctl = device_ioctl;
    i->ops.fsync = device_fsync;
    


    switch(device->type) {
        case DEVICE_TYPE_CHAR:
                        
            break;

        case DEVICE_TYPE_BLOCK:

            block_inode(device, i, device_mkdev /* FIXME */);
            break;

        case DEVICE_TYPE_VIDEO:

            break;

        default:
            kpanicf("device::create: failed, unknown device %s type %d\n", device->name, device->type);
    }



#if defined(DEBUG)
    kprintf("device::create: initialized '%s' dev(%x:%x) addr(%p) size(%p): '%s'\n",
        device->name, 
        device->major, 
        device->minor, 
        device->address,
        device->size,
        device->description);
#endif

    list_push(devices, device);

}


void device_unlink(device_t* device) {
    DEBUG_ASSERT(device);

    device->status = DEVICE_STATUS_UNLOADING;

    if(likely(device->dnit))
        device->dnit(device);


    switch(device->type) {
        case DEVICE_TYPE_CHAR:
            
            char_dnit(device);
            break;

        case DEVICE_TYPE_BLOCK:

            block_dnit(device);
            break;

        case DEVICE_TYPE_VIDEO:

            video_dnit(device);
            break;

        default:
            kpanicf("device::unlink: failed, unknown device %s type %d\n", device->name, device->type);
    }

    device->status = DEVICE_STATUS_UNLOADED;



    char buf[MAXNAMLEN] = { 0 };
    strcpy(buf, "/dev/");
    strcpy(buf, device->name);

    if(sys_unlink(buf) < 0)
        kpanicf("device::unlink: failed to remove device inode\n");

    list_remove(devices, device);

}


void device_reset(device_t* device) {

    DEBUG_ASSERT(device);


    device->status = DEVICE_STATUS_LOADING;

    if(likely(device->reset))
        device->reset(device);

    if(unlikely(device->status == DEVICE_STATUS_FAILED))
        kpanicf("device::reset: fail on resetting %s\n", device->name);

    device->status = DEVICE_STATUS_READY;
}


void device_error(device_t* device, const char* errstr) {

    kprintf("device::error: %s: '%s'\n", device->name, errstr);


    device->status = DEVICE_STATUS_LOADING;

    if(likely(device->reset))
        device->reset(device);

    if(unlikely(device->status != DEVICE_STATUS_FAILED))
        device->status = DEVICE_STATUS_READY;
    

    if(device->status == DEVICE_STATUS_FAILED)
        kprintf("device::error: %s: device failed!\n", device->name);
    else
        kprintf("device::error: %s: device restarted, ready!\n", device->name);
}


void init(const char* args) {
    
    memset(&devices, 0, sizeof(devices));

    int e;
    if((e = sys_mkdir("/dev", S_IFDIR | 0666)) < 0)
        kpanicf("dev: could not create /dev directory: errno(%d)\n", -e);

}


void dnit(void) {

    list_each(devices, d)
        device_unlink(d);

}