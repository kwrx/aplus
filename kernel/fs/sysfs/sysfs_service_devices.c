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

#include <dirent.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/ipc.h>
#include <aplus/memory.h>
#include <aplus/vfs.h>

#include "sysfs.h"


static inode_t* sysfs_device_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(name);

    sysfs_service_t* service = inode->userdata;

    if (strcmp(name, "uevent") != 0)
        return errno = ENOENT, NULL;

    scoped_lock(&sysfs_bus_lock) {

        sysfs_device_t* dev = service->arg;

        if (dev == NULL)
            return errno = ENOENT, NULL;

        if (dev->file == NULL) {
            dev->file              = sysfs_service_inode(inode, "uevent", S_IFREG | 0444, sysfs_service_text_fetch, dev->uevent);
            dev->file->ino         = SYSFS_INO_DEVICE_FILE(dev->id, SYSFS_DEVICE_SLOT_UEVENT);
            dev->file->ops.release = sysfs_service_text_release;
        }

        return dev->file;
    }

    return errno = ENOENT, NULL;
}

static ssize_t sysfs_device_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(e);

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;

    sysfs_service_t* service = inode->userdata;

    bool alive = false;

    scoped_lock(&sysfs_bus_lock) {
        alive = service->arg != NULL;
    }

    size_t i = 0;

    for (size_t j = (size_t)pos; j < (alive ? 3 : 2) && i < count; j++, i++) {

        switch (j) {
            case 0:
                sysfs_dirent(&e[i], inode->ino, DT_DIR, ".", (off_t)i);
                break;
            case 1:
                sysfs_dirent(&e[i], inode->parent->ino, DT_DIR, "..", (off_t)i);
                break;
            default:
                sysfs_dirent(&e[i], SYSFS_INO_DEVICE_FILE(inode->ino >> 8, SYSFS_DEVICE_SLOT_UEVENT), DT_REG, "uevent", (off_t)i);
                break;
        }
    }

    return (ssize_t)i;
}


static inode_t* sysfs_devices_root_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(name);

    sysfs_bus_t* bus = ((sysfs_service_t*)inode->userdata)->arg;

    DEBUG_ASSERT(bus);

    scoped_lock(&sysfs_bus_lock) {

        sysfs_device_t* dev = sysfs_bus_device_find(bus, name);

        if (dev == NULL)
            return errno = ENOENT, NULL;

        if (dev->dir == NULL) {
            dev->dir              = sysfs_service_inode(inode, dev->name, S_IFDIR | 0555, NULL, dev);
            dev->dir->ino         = SYSFS_INO_DEVICE(dev->id);
            dev->dir->ops.finddir = sysfs_device_finddir;
            dev->dir->ops.readdir = sysfs_device_readdir;
            dev->dir->ops.release = sysfs_service_release;
        }

        return dev->dir;
    }

    return errno = ENOENT, NULL;
}

static ssize_t sysfs_devices_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(inode->userdata);
    DEBUG_ASSERT(e);

    return sysfs_bus_list_devices(inode, ((sysfs_service_t*)inode->userdata)->arg, e, pos, count, DT_DIR);
}


static inode_t* sysfs_service_devices_finddir(inode_t* inode, const char* name) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(name);

    for (size_t i = 0; i < sysfs_buses_count; i++) {

        sysfs_bus_t* bus = &sysfs_buses[i];

        if (strcmp(bus->root, name) != 0)
            continue;

        if (bus->rootdir == NULL) {

            inode_t* d     = sysfs_service_inode(inode, bus->root, S_IFDIR | 0555, NULL, bus);
            d->ino         = SYSFS_INO_STATIC(SYSFS_BUS_SLOT_ROOT(i));
            d->ops.finddir = sysfs_devices_root_finddir;
            d->ops.readdir = sysfs_devices_root_readdir;

            scoped_lock(&sysfs_bus_lock) {
                bus->rootdir = d;
            }
        }

        return bus->rootdir;
    }

    return errno = ENOENT, NULL;
}

static ssize_t sysfs_service_devices_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count) {

    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(e);

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;

    size_t i = 0;

    for (size_t j = (size_t)pos; j < sysfs_buses_count + 2 && i < count; j++, i++) {

        if (j == 0)
            sysfs_dirent(&e[i], SYSFS_INO_STATIC(SYSFS_DEVICES_SLOT_DIR), DT_DIR, ".", (off_t)i);
        else if (j == 1)
            sysfs_dirent(&e[i], SYSFS_INO_ROOT, DT_DIR, "..", (off_t)i);
        else
            sysfs_dirent(&e[i], SYSFS_INO_STATIC(SYSFS_BUS_SLOT_ROOT(j - 2)), DT_DIR, sysfs_buses[j - 2].root, (off_t)i);
    }

    return (ssize_t)i;
}


inode_t* sysfs_service_devices_inode(inode_t* parent) {

    static inode_t* inode = NULL;

    if (inode == NULL) {
        inode              = sysfs_service_inode(parent, "devices", S_IFDIR | 0555, NULL, NULL);
        inode->ino         = SYSFS_INO_STATIC(SYSFS_DEVICES_SLOT_DIR);
        inode->ops.finddir = sysfs_service_devices_finddir;
        inode->ops.readdir = sysfs_service_devices_readdir;
    }

    return inode;
}
