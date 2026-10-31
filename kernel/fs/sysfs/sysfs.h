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


#ifndef _SYSFS_H
#define _SYSFS_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>

#define SYSFS_INO_ROOT               ((ino_t)1)
#define SYSFS_INO_STATIC(slot)       ((ino_t)(2 + (slot)))
#define SYSFS_INO_DEVICE(id)         ((((ino_t)(id)) << 8) | 0xFF)
#define SYSFS_INO_DEVICE_FILE(id, s) ((((ino_t)(id)) << 8) | ((ino_t)(s)))

#define SYSFS_BUS_SLOT_DIR          1
#define SYSFS_DEVICES_SLOT_DIR      2
#define SYSFS_BUS_SLOT(bus)         (3 + ((bus) * 3))
#define SYSFS_BUS_SLOT_DEVICES(bus) (4 + ((bus) * 3))
#define SYSFS_BUS_SLOT_ROOT(bus)    (5 + ((bus) * 3))

#define SYSFS_DEVICE_SLOT_UEVENT 1
#define SYSFS_DEVICE_SLOT_LINK   2


typedef struct sysfs_service {
    int (*fetch)(inode_t*, char**, size_t*, void*);
    void* arg;
    mode_t mode;
} sysfs_service_t;

typedef struct sysfs_device {
    char name[CONFIG_MAXNAMLEN];
    char* uevent;
    uint32_t id;
    inode_t* dir;
    inode_t* file;
    inode_t* link;
    struct sysfs_device* next;
} sysfs_device_t;

typedef struct sysfs_bus {
    const char* name;
    const char* root;
    sysfs_device_t* devices;
    inode_t* dir;
    inode_t* devdir;
    inode_t* rootdir;
} sysfs_bus_t;


__BEGIN_DECLS

extern sysfs_bus_t sysfs_buses[];
extern const size_t sysfs_buses_count;
extern spinlock_t sysfs_bus_lock;

int sysfs_root_getattr(inode_t* inode, struct stat* st);
inode_t* sysfs_root_finddir(inode_t* inode, const char* name);
ssize_t sysfs_root_readdir(inode_t* inode, struct dirent* e, off_t pos, size_t count);

inode_t* sysfs_service_inode(inode_t* parent, const char* name, mode_t mode, int (*fetch)(inode_t*, char** buf, size_t*, void*), void* arg);
void sysfs_service_release(inode_t* inode);
void sysfs_dirent(struct dirent* e, ino_t ino, unsigned char type, const char* name, off_t off);
int sysfs_service_text_fetch(inode_t* inode, char** buf, size_t* size, void* arg);
void sysfs_service_text_release(inode_t* inode);

sysfs_bus_t* sysfs_bus_find(const char* name);
sysfs_device_t* sysfs_bus_device_find(sysfs_bus_t* bus, const char* name);
ssize_t sysfs_bus_list_devices(inode_t* inode, sysfs_bus_t* bus, struct dirent* e, off_t pos, size_t count, unsigned char type);

inode_t* sysfs_service_bus_inode(inode_t* parent);
inode_t* sysfs_service_devices_inode(inode_t* parent);

__END_DECLS

#endif
