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
#include <aplus/sysfs.h>
#include <aplus/vfs.h>

#include "sysfs.h"


sysfs_bus_t sysfs_buses[] = {
    {.name = "pci", .root = "pci0000:00"},
    {.name = "usb", .root = "usb"       },
};

const size_t sysfs_buses_count = sizeof(sysfs_buses) / sizeof(sysfs_buses[0]);

spinlock_t sysfs_bus_lock = {0};

static uint32_t sysfs_device_next_id = 1;


sysfs_bus_t* sysfs_bus_find(const char* name) {

    for (size_t i = 0; i < sysfs_buses_count; i++) {

        if (strcmp(sysfs_buses[i].name, name) == 0)
            return &sysfs_buses[i];
    }

    return NULL;
}

sysfs_device_t* sysfs_bus_device_find(sysfs_bus_t* bus, const char* name) {

    for (sysfs_device_t* d = bus->devices; d; d = d->next) {

        if (strcmp(d->name, name) == 0)
            return d;
    }

    return NULL;
}


ssize_t sysfs_bus_list_devices(inode_t* inode, sysfs_bus_t* bus, struct dirent* e, off_t pos, size_t count, unsigned char type) {

    if (unlikely(pos < 0))
        return errno = EINVAL, -1;

    size_t i = 0;

    if (pos == 0 && i < count) {
        sysfs_dirent(&e[i], inode->ino, DT_DIR, ".", (off_t)i);
        i++;
    }

    if (pos <= 1 && i < count) {
        sysfs_dirent(&e[i], inode->parent->ino, DT_DIR, "..", (off_t)i);
        i++;
    }

    scoped_lock(&sysfs_bus_lock) {

        off_t j = 2;

        for (sysfs_device_t* d = bus->devices; d && i < count; d = d->next, j++) {

            if (j < pos)
                continue;

            sysfs_dirent(&e[i], type == DT_LNK ? SYSFS_INO_DEVICE_FILE(d->id, SYSFS_DEVICE_SLOT_LINK) : SYSFS_INO_DEVICE(d->id), type, d->name, (off_t)i);
            i++;
        }
    }

    return (ssize_t)i;
}


int sysfs_device_bus_add(const char* bus, const char* name, const char* uevent) {

    DEBUG_ASSERT(bus);
    DEBUG_ASSERT(name);
    DEBUG_ASSERT(uevent);

    sysfs_bus_t* b = sysfs_bus_find(bus);

    if (unlikely(b == NULL))
        return errno = ENOENT, -1;

    size_t len = strlen(name);

    if (unlikely(len == 0 || len >= CONFIG_MAXNAMLEN || strchr(name, '/') || strcmp(name, ".") == 0 || strcmp(name, "..") == 0))
        return errno = EINVAL, -1;


    sysfs_device_t* dev = kcalloc(1, sizeof(sysfs_device_t), GFP_KERNEL);

    memcpy(dev->name, name, len + 1);

    dev->uevent = kmalloc(strlen(uevent) + 1, GFP_KERNEL);
    strcpy(dev->uevent, uevent);


    scoped_lock(&sysfs_bus_lock) {

        if (sysfs_bus_device_find(b, name) == NULL) {

            dev->id = sysfs_device_next_id++;

            sysfs_device_t** tail = &b->devices;

            while (*tail)
                tail = &(*tail)->next;

            *tail = dev;
            dev   = NULL;
        }
    }

    if (dev) {
        kfree(dev->uevent);
        kfree(dev);
        return errno = EEXIST, -1;
    }

    return 0;
}

int sysfs_device_bus_remove(const char* bus, const char* name) {

    DEBUG_ASSERT(bus);
    DEBUG_ASSERT(name);

    sysfs_bus_t* b = sysfs_bus_find(bus);

    if (unlikely(b == NULL))
        return errno = ENOENT, -1;


    sysfs_device_t* dev = NULL;
    inode_t* devdir     = NULL;
    inode_t* rootdir    = NULL;

    scoped_lock(&sysfs_bus_lock) {

        for (sysfs_device_t** p = &b->devices; *p; p = &(*p)->next) {

            if (strcmp((*p)->name, name) != 0)
                continue;

            dev = *p;
            *p  = dev->next;

            if (dev->dir)
                ((sysfs_service_t*)dev->dir->userdata)->arg = NULL;

            break;
        }

        devdir  = b->devdir;
        rootdir = b->rootdir;
    }

    if (dev == NULL)
        return errno = ENOENT, -1;


    if (dev->link) {

        DEBUG_ASSERT(devdir);

        spinlock_lock(&devdir->lock);
        spinlock_unlock(&devdir->lock);

        vfs_inode_unlink(dev->link);
    }

    if (dev->dir) {

        DEBUG_ASSERT(rootdir);

        spinlock_lock(&rootdir->lock);
        spinlock_unlock(&rootdir->lock);

        spinlock_lock(&dev->dir->lock);
        spinlock_unlock(&dev->dir->lock);

        if (dev->file)
            vfs_inode_unlink(dev->file);

        vfs_inode_unlink(dev->dir);
    }

    if (dev->file == NULL)
        kfree(dev->uevent);

    kfree(dev);

    return 0;
}


void sysfs_init(void) {
    spinlock_init(&sysfs_bus_lock);
}
