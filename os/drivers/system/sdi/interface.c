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
#include <aplus/vfs.h>
#include <stdint.h>
#include <errno.h>

#include <sdi/device.h>
#include <sdi/chrdev.h>

#include <aplus/utils/list.h>



static int sdi_read(inode_t* inode, void __user * buf, off_t off, size_t size) {
    (void) off;
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if(unlikely(!ptr_check(buf, R_OK | W_OK)))
        return -EFAULT;


    device_t* device = (device_t*) inode->userdata;

    DEBUG_ASSERT(device->magic == DEVICE_MAGIC);


    switch(device->type) {
        
        case DEVICE_TYPE_CHRDEV:
            return chrdev_read(&device->chrdev, buf, size);

        
    }


    kpanic("device::read: unknown device type %d for /dev/%s\n", device->type, inode->name);
}

static int sdi_write(inode_t* inode, const void __user * buf, off_t off, size_t size) {
    (void) off;
    
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if(unlikely(!ptr_check(buf, R_OK)))
        return -EFAULT;


    device_t* device = (device_t*) inode->userdata;

    DEBUG_ASSERT(device->magic == DEVICE_MAGIC);


    switch(device->type) {
        
        case DEVICE_TYPE_CHRDEV:
            return chrdev_write(&device->chrdev, buf, size);

        
    }


    kpanic("device::write: unknown device type %d for /dev/%s\n", device->type, inode->name);

}

static int sdi_ioctl(inode_t* inode, int req, void __user * arg) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(arg);

    if(unlikely(!ptr_check(arg, R_OK | W_OK)))
        return -EFAULT;


    device_t* device = (device_t*) inode->userdata;

    DEBUG_ASSERT(device->magic == DEVICE_MAGIC);


    switch(device->type) {
        
        case DEVICE_TYPE_CHRDEV:
            return -ENOSYS; /* TODO */

        
    }


    kpanic("device::ioctl: unknown device type %d for /dev/%s\n", device->type, inode->name);
}

static int sdi_fsync(inode_t* inode) {
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);


    device_t* device = (device_t*) inode->userdata;

    DEBUG_ASSERT(device->magic == DEVICE_MAGIC);


    switch(device->type) {
        
        case DEVICE_TYPE_CHRDEV:
            return chrdev_flush(&device->chrdev);

        
    }


    kpanic("device::fsync: unknown device type %d for /dev/%s\n", device->type, inode->name);
}

#if 0
void sdi_device_add(int type, void* device, mode_t mode) {
    DEBUG_ASSERT(device);

    device_t* d = kmalloc(sizeof(device_t), GFP_KERNEL);
    d->type = type;

    switch(d->type) {
        case DEVICE_TYPE_CHRDEV:
            memcpy(&d->chrdev, device, sizeof(chrdev_t));

            if(likely(d->chrdev.ops.init))
                d->chrdev.ops.init(&d->chrdev);

            if(likely(d->chrdev.ops.reset))
                d->chrdev.ops.reset(&d->chrdev);
            
            
            kprintf("sdi: new char device %s\n", d->chrdev.name);
            break;

        default:
            kpanic("sdi: unknown device type %d\n", type);
    }

    list_push(devices, d);
}
#endif