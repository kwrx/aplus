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
#include <sys/types.h>
#include <sys/mount.h>

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/smp.h>
#include <aplus/ipc.h>
#include <aplus/vfs.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <aplus/endian.h>

#include <aplus/utils/list.h>

#include "iso9660.h"




int iso9660_mount(inode_t* dev, inode_t* dir, int flags, const char * args) {
    
    DEBUG_ASSERT(dir);
    DEBUG_ASSERT(dev);

    (void) args;


    #define __(a, b)                        \
        if(flags & a)                       \
            stflags |= b

        int stflags = 0;

        __(MS_MANDLOCK, ST_MANDLOCK);
        __(MS_NOATIME, ST_NOATIME);
        __(MS_NODEV, ST_NODEV);
        __(MS_NODIRATIME, ST_NODIRATIME);
        __(MS_NOEXEC, ST_NOEXEC);
        __(MS_NOSUID, ST_NOSUID);
        __(MS_RDONLY, ST_RDONLY);
        __(MS_SYNCHRONOUS, ST_SYNCHRONOUS);

    #undef __


    iso9660_t* iso9660 = (iso9660_t*) kcalloc(sizeof(iso9660_t), 1, GFP_USER);

    iso9660->dev = dev;
    iso9660->dir = dir;


    for(size_t block = 16; block < 32; block++) {

        iso9660_volume_descriptor_t vd = { 0 };
        
        if(vfs_read(dev, &vd, ISO9660_BLOCK_SIZE * block, sizeof(iso9660_volume_descriptor_t)) != sizeof(iso9660_volume_descriptor_t))
            return errno = EIO, -1;


        switch(vd.type) {

            case ISO9660_VOLUME_DESCRIPTOR_TYPE_BOOT_RECORD:

#if DEBUG_LEVEL_TRACE

                vd.boot_record.boot_system_id[31] = 0;
                vd.boot_record.boot_id[31] = 0;

                kprintf (
                    "iso9660: found ISO9660_VOLUME_DESCRIPTOR_BOOT_RECORD at sector %ld\n"
                    " - boot system id: %s\n"
                    " - boot id: %s\n",
                    block,
                    (char*) vd.boot_record.boot_system_id,
                    (char*) vd.boot_record.boot_id
                );

#endif
                break;

            case ISO9660_VOLUME_DESCRIPTOR_TYPE_PRIMARY:
    
                iso9660->block_size = le16_to_cpu(vd.primary.logical_block_size.lsb);
                iso9660->blocks     = le32_to_cpu(vd.primary.volume_space_size.lsb);

                memcpy(&iso9660->root, &vd.primary.root_directory_record, sizeof(iso9660_directory_record_t));


#if DEBUG_LEVEL_TRACE

                vd.primary.system_id[31] = 0;
                vd.primary.volume_id[31] = 0;
                vd.primary.volume_set_id[127] = 0;
                vd.primary.publisher_id[127] = 0;
                vd.primary.preparer_id[127] = 0;
                vd.primary.application_id[127] = 0;

                kprintf (
                    "iso9660: found ISO9660_VOLUME_DESCRIPTOR_TYPE_PRIMARY at sector %ld\n"
                    " - system id: %s\n"
                    " - volume id: %s\n"
                    " - volume space size: %d\n"
                    " - volume set size: %d\n"
                    " - volume sequence number: %d\n"
                    " - logical block size: %d\n"
                    " - path table size: %d\n"
                    " - path table location: %d\n"
                    " - path table location (optional): %d\n"
                    " - volume set id: %s\n"
                    " - publisher id: %s\n"
                    " - data preparer id: %s\n"
                    " - application id: %s\n",
                    block,
                    (char*) vd.primary.system_id,
                    (char*) vd.primary.volume_id,
                    le32_to_cpu(vd.primary.volume_space_size.lsb),
                    le16_to_cpu(vd.primary.volume_set_size.lsb),
                    le16_to_cpu(vd.primary.volume_sequence_number.lsb),
                    le16_to_cpu(vd.primary.logical_block_size.lsb),
                    le16_to_cpu(vd.primary.path_table_size.lsb),
                    le32_to_cpu(vd.primary.type_l_path_table),
                    le32_to_cpu(vd.primary.type_l_path_table_opt),
                    (char*) vd.primary.volume_set_id,
                    (char*) vd.primary.publisher_id,
                    (char*) vd.primary.preparer_id,
                    (char*) vd.primary.application_id
                );

#endif
                break;

            case ISO9660_VOLUME_DESCRIPTOR_TYPE_SUPPLEMENTARY:

#if DEBUG_LEVEL_TRACE
                kprintf("iso9660: found ISO9660_VOLUME_DESCRIPTOR_TYPE_SUPPLEMENTARY at sector %ld\n", block);
#endif
                break;

            case ISO9660_VOLUME_DESCRIPTOR_TYPE_PARTITION:

#if DEBUG_LEVEL_TRACE
                kprintf("iso9660: found ISO9660_VOLUME_DESCRIPTOR_TYPE_PARTITION at sector %ld\n", block);
#endif
                break;      

            case ISO9660_VOLUME_DESCRIPTOR_TYPE_TERMINATOR:

#if DEBUG_LEVEL_TRACE
                kprintf("iso9660: found ISO9660_VOLUME_DESCRIPTOR_TYPE_TERMINATOR at sector %ld\n", block);
#endif
                break;

            default:

#if DEBUG_LEVEL_ERROR
                kprintf("iso9660: ERROR! found unknown volume descriptor at sector %ld of type %d\n", block, vd.type);
#endif
                return errno = EIO, -1;

        }

        if(vd.type == ISO9660_VOLUME_DESCRIPTOR_TYPE_TERMINATOR)
            break;

    }



    if(!iso9660->block_size || !iso9660->blocks) {

#if DEBUG_LEVEL_ERROR
        kprintf("iso9660: ERROR! invalid block size or block count\n");
#endif

        return errno = EIO, -1;

    }


    DEBUG_ASSERT(iso9660->block_size == ISO9660_BLOCK_SIZE);


    dir->sb = (struct superblock*) kcalloc(sizeof(struct superblock), 1, GFP_KERNEL);

    dir->sb->fsid   = FSID_ISO9660;
    dir->sb->dev    = dev;
    dir->sb->root   = dir;
    dir->sb->flags  = flags;
    dir->sb->fsinfo = iso9660;

    dir->sb->st.f_bsize   = iso9660->block_size;
    dir->sb->st.f_frsize  = 1;
    dir->sb->st.f_blocks  = iso9660->blocks;
    dir->sb->st.f_bfree   = 0;
    dir->sb->st.f_bavail  = 0;
    dir->sb->st.f_files   = 0;
    dir->sb->st.f_ffree   = 0;
    dir->sb->st.f_favail  = 0;
    dir->sb->st.f_flag    = stflags;
    dir->sb->st.f_fsid    = FSID_ISO9660;
    dir->sb->st.f_namemax = ISO9660_MAX_NAME;


    dir->sb->ops.getattr = iso9660_getattr;
    dir->sb->ops.finddir = iso9660_finddir;
    dir->sb->ops.readdir = iso9660_readdir;


    struct cache_ops ops;
    ops.load = (cache_load_handler_t) iso9660_cache_load;
    ops.sync = (cache_sync_handler_t) iso9660_cache_sync;

    cache_init(&dir->sb->cache, &ops, iso9660);


    dir->sb->ino = dir->ino;

    return 0;

}