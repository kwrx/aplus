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

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/sysmacros.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/module.h>

#include <dev/char.h>
#include <dev/interface.h>



MODULE_NAME("dev/char");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");



int char_getattr(device_t *device, struct stat *st) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(st);

    st->st_dev     = makedev(device->major, device->minor);
    st->st_ino     = device->inode->ino;
    st->st_mode    = S_IFCHR | 0666;
    st->st_nlink   = 1;
    st->st_uid     = 0;
    st->st_gid     = 0;
    st->st_rdev    = makedev(device->major, device->minor);
    st->st_size    = 0;
    st->st_blksize = 0;
    st->st_blocks  = 0;
    st->st_atime   = arch_timer_gettime();
    st->st_mtime   = arch_timer_gettime();
    st->st_ctime   = arch_timer_gettime();

    return 0;
}


ssize_t char_write(device_t *device, const void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    int e;

    switch (device->chr.io) {

        case CHAR_IO_NBF:

            if (likely(device->chr.write))
                return device->chr.write(device, buf, size);

            return errno = ENOSYS, -1;


        case CHAR_IO_LBF:

            DEBUG_ASSERT(device->chr.buffer.buffer);
            DEBUG_ASSERT(device->chr.buffer.size);

            e = ringbuffer_write(&device->chr.buffer, buf, size);

            if (memchr(buf, '\n', size))
                if (likely(device->chr.flush))
                    device->chr.flush(device);

            return e;


        case CHAR_IO_FBF:

            DEBUG_ASSERT(device->chr.buffer.buffer);
            DEBUG_ASSERT(device->chr.buffer.size);

            e = ringbuffer_write(&device->chr.buffer, buf, size);


            if (ringbuffer_is_full(&device->chr.buffer)) {

                if (likely(device->chr.flush)) {
                    device->chr.flush(device);
                }
            }

            return e;


        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");

    return errno = EIO, -1;
}



ssize_t char_read(device_t *device, void *buf, size_t size) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(buf);
    DEBUG_ASSERT(size);


    if (device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;


    switch (device->chr.io) {

        case CHAR_IO_NBF:

            if (likely(device->chr.read))
                return device->chr.read(device, buf, size);

            return errno = ENOSYS, -1;


        case CHAR_IO_LBF:
        case CHAR_IO_FBF:

            DEBUG_ASSERT(device->chr.buffer.buffer);
            DEBUG_ASSERT(device->chr.buffer.size);

            return ringbuffer_read(&device->chr.buffer, buf, size);


        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");

    return errno = EIO, -1;
}



int char_flush(device_t *device) {

    DEBUG_ASSERT(device);

    if (device->status != DEVICE_STATUS_READY)
        return errno = EBUSY, -1;



    switch (device->chr.io) {

        case CHAR_IO_NBF:
            return 0;


        case CHAR_IO_LBF:
        case CHAR_IO_FBF:

            DEBUG_ASSERT(device->chr.buffer.buffer);
            DEBUG_ASSERT(device->chr.buffer.size);

            if (likely(device->chr.flush))
                device->chr.flush(device);

            return 0;


        default:
            break;
    }

    DEBUG_ASSERT(0 && "Bug: Invalid DEVICE_IO_*BF");

    return errno = EIO, -1;
}


void char_init(device_t *device) {

    DEBUG_ASSERT(device);

    switch (device->chr.io) {

        case CHAR_IO_FBF:
        case CHAR_IO_LBF:

            ringbuffer_init(&device->chr.buffer, BUFSIZ);


        default:
            break;
    }
}


void char_dnit(device_t *device) {

    DEBUG_ASSERT(device);

    switch (device->chr.io) {

        case CHAR_IO_FBF:
        case CHAR_IO_LBF:

            ringbuffer_destroy(&device->chr.buffer);


        default:
            break;
    }
}


void init(const char *args) {
    (void)args;
}

void dnit(void) {
}
