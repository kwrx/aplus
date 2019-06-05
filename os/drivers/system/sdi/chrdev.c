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

#include <sdi/device.h>
#include <sdi/chrdev.h>



void chrdev_add(chrdev_t* device, mode_t mode) {
    DEBUG_ASSERT(device);


    device->status = DEVICE_STATUS_LOADING;

    if(likely(device->ops.init))
        device->ops.init(device);

    if(likely(device->ops.reset))
        device->ops.reset(device);

    if(unlikely(device->status == DEVICE_STATUS_FAILED))
        kpanic("chrdev: fail on loading %s: %s\n", device->name, device->description);

    device->status = DEVICE_STATUS_READY;

}


void chrdev_remove(chrdev_t* device) {
    DEBUG_ASSERT(device);
    

    device->status = DEVICE_STATUS_UNLOADING;

    if(likely(device->ops.init))
        device->ops.dnit(device);

    device->status = DEVICE_STATUS_UNLOADED;
}

int chrdev_write(chrdev_t* device, const void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if(device->status != DEVICE_STATUS_READY)
        return -EBUSY;



    int e;

    switch(device->io) {
        case CHRDEV_IO_NBF:
            if(likely(device->ops.write))
                return device->ops.write(device, buf, size);

            return -ENOSYS;

        case CHRDEV_IO_LBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            e = ringbuffer_write(&device->buffer, buf, size);
            
            if(memchr(buf, '\n', size))
                if(likely(device->ops.flush))
                    device->ops.flush(device);

            return e;
        
        case CHRDEV_IO_FBF:
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


int chrdev_read(chrdev_t* device, void* buf, size_t size) {
    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if(device->status != DEVICE_STATUS_READY)
        return -EBUSY;



    int e;

    switch(device->io) {
        case CHRDEV_IO_NBF:
            if(likely(device->ops.read))
                return device->ops.read(device, buf, size);

            return -ENOSYS;

        case CHRDEV_IO_LBF:
        case CHRDEV_IO_FBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            return ringbuffer_read(&device->buffer, buf, size);
            
        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");
}


int chrdev_flush(chrdev_t* device) {
    DEBUG_ASSERT(device);

    if(device->status != DEVICE_STATUS_READY)
        return -EBUSY;



    int e;

    switch(device->io) {
        case CHRDEV_IO_NBF:
            return 0;

        case CHRDEV_IO_LBF:
        case CHRDEV_IO_FBF:
            DEBUG_ASSERT(device->buffer.buffer);
            DEBUG_ASSERT(device->buffer.size);

            if(likely(device->ops.flush))
                device->ops.flush(device);    

            return 0;
                    
        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");
}