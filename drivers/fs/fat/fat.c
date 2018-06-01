/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
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
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <libc.h>

#include "fat.h"


static int fatfs_dev_read(struct fatfs* fs, uint32_t sector, uint8_t* buffer, uint32_t sector_count) {
    return vfs_read((inode_t*) fs->disk_io.userdata, (void*) buffer, sector * FAT_SECTOR_SIZE, sector_count * FAT_SECTOR_SIZE);
}

static int fatfs_dev_write(struct fatfs* fs, uint32_t sector, uint8_t* buffer, uint32_t sector_count) {
    return vfs_write((inode_t*) fs->disk_io.userdata, (void*) buffer, sector * FAT_SECTOR_SIZE, sector_count * FAT_SECTOR_SIZE);
}

char* fat_getname(struct fat_dir_entry* e, char* lfn) {
    char* get_sfn(char* sfn) {
        char s[13];
        memset(s, 0, sizeof(s));


        for(int i = 0; i < 8; i++)
            s[i] = sfn[i];
        for(int i = 8; i < 11; i++)
            s[i + 1] = sfn[i];

        if(sfn[8] != ' ' && sfn[0] != '.')
            s[8] = '.';
        else
            s[8] = ' ';

        char r[13];
        fatfs_get_sfn_display_name(r, s);
        return strdup(r);
    }


    return (
        lfn 
            ? strdup(lfn) 
            : get_sfn(e->Name)
    );
}


inode_t* fat_mkchild(inode_t* parent, struct fat_dir_entry* e, char* name, mode_t mode) {
    inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_KERNEL);
    if(unlikely(!child)) {
        kprintf(ERROR "vfat: no memory left!\n");
        return NULL;
    }

    memset(child, 0, sizeof(inode_t));

    if(likely(e))
        mode = fatfs_entry_is_dir(e)
            ? S_IFDIR | 0666
            : S_IFREG | 0777;

    child->name = name;
    child->ino = vfs_inode();
    child->mode = mode & ~current_task->umask;

    child->dev =
    child->rdev =
    child->nlink = 0;
    child->uid = current_task->uid;
    child->gid = current_task->gid;
    child->size = e ? FAT_HTONL(e->FileSize) : 0;

    child->atime = 
    child->ctime = 
    child->mtime = timer_gettimestamp();
    
    child->parent = parent;
    child->mtinfo = parent->mtinfo;
    child->link = NULL;

    child->childs = NULL;

    if(S_ISDIR(child->mode)) {
        child->open = fat_open;
        child->finddir = fat_finddir;
        child->mknod = fat_mknod;
        child->unlink = fat_unlink;
    } else {
        child->read = fat_read;
        child->write = fat_write;
    }

    child->fsync = fat_fsync;
    child->chown = NULL;
    child->chmod = NULL;
    child->ioctl = NULL;



    fat_t* fat = (fat_t*) kmalloc(sizeof(fat_t), GFP_KERNEL);
    if(unlikely(!fat)) {
        errno = ENOMEM;
        return NULL;
    }

    fat->sb = parent->userdata 
                ? ((fat_t*) parent->userdata)->sb 
                : NULL
                ;

    fat->cluster = e
                ? ((FAT_HTONS((uint32_t) e->FstClusHI)) << 16) | FAT_HTONS(e->FstClusLO)
                : 0
                ;

    if(!S_ISDIR(child->mode) && fat->sb)
        fatfs_cache_init(fat->sb, &fat->cache);

    child->userdata = (void*) fat;

    //parent->mtinfo->stat.f_ffree--;
    //parent->mtinfo->stat.f_favail--;

    return child;
}

int fat_mount(struct inode* dev, struct inode* dir, struct mountinfo* info) {

    struct fatfs* fs = (struct fatfs*) kmalloc(sizeof(struct fatfs), GFP_KERNEL);
    if(unlikely(!fs)) {
        errno = ENOMEM;
        return -1;
    }

    fs->fl_lock =
    fs->fl_unlock = NULL;
    fs->disk_io.read_media = fatfs_dev_read;
    fs->disk_io.write_media = fatfs_dev_write;
    fs->disk_io.userdata = (void*) dev;

    if(fatfs_init(fs) != FAT_INIT_OK) {
        errno = EINVAL;
        return -1;
    }


    fat_t* fat = (fat_t*) kmalloc(sizeof(fat_t), GFP_KERNEL);
    if(unlikely(!fat)) {
        errno = ENOMEM;
        return -1;
    }

    fat->sb = fs;
    fat->cluster = fatfs_get_root_cluster(fs);

    fatfs_cache_init(fs, &fat->cache);


    dir->open = fat_open;
    dir->close = NULL;
    dir->unlink = fat_unlink;
    dir->mknod = fat_mknod;
    dir->finddir = NULL;
    dir->userdata = (void*) fat;
    dir->mtinfo = info;



    info->stat.f_bsize = fs->sectors_per_cluster * FAT_SECTOR_SIZE;
    info->stat.f_frsize = FAT_SECTOR_SIZE;
    info->stat.f_blocks =
    info->stat.f_bfree =
    info->stat.f_bavail = 0;
    info->stat.f_files =
    info->stat.f_ffree =
    info->stat.f_favail = 0;
    info->stat.f_namemax = FATFS_MAX_LONG_FILENAME;


    return 0;
}
