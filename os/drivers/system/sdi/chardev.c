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
#include <stdint.h>
#include <errno.h>

#include <sdi/driver.h>
#include <sdi/chardev.h>


int chardev_write(chardev_t* device, void __user * buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if(unlikely(!ptr_check(buf, R_OK)))
        return -EFAULT;

    if(device->status != DRIVER_STATUS_READY)
        return -EBUSY;



    int e;

    switch(device->io) {
        case CHARDEV_IO_NBF:
            if(likely(device->ops.write))
                return device->ops.write(device, buf, size);

            return -ENOSYS;

        case CHARDEV_IO_LBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            e = ringbuffer_write(&device->buffer, buf, size);
            
            if(memchr(buf, '\n', size))
                if(likely(device->ops.flush))
                    device->ops.flush(device);

            return e;
        
        case CHARDEV_IO_FBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            e = ringbuffer_write(&device->buffer, buf, size);

            if(ringbuffer_is_full(&device->buffer))
                if(likely(device->ops.flush))
                    device->ops.flush(device);

            return e;

        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");
}


int chardev_read(chardev_t* device, void __user * buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);

    if(unlikely(!ptr_check(buf, R_OK | W_OK)))
        return -EFAULT;

    if(device->status != DRIVER_STATUS_READY)
        return -EBUSY;



    int e;

    switch(device->io) {
        case CHARDEV_IO_NBF:
            if(likely(device->ops.read))
                return device->ops.read(device, buf, size);

            return -ENOSYS;

        case CHARDEV_IO_LBF:
        case CHARDEV_IO_FBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            return ringbuffer_read(&device->buffer, buf, size);
            
        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");
}