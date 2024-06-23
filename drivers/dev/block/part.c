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


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/errno.h>
#include <aplus/memory.h>
#include <aplus/module.h>
#include <aplus/smp.h>
#include <stdint.h>
#include <stdio.h>

#include <dev/block.h>
#include <dev/interface.h>


static bool detect_gpt_partition_table(device_t *device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);


    char efi[8] = {0};

    if (block_read(device, &efi, BLOCK_GPT_PARTITION_MAGIC_LBA * device->blk.blksize, sizeof(efi)) != sizeof(efi))
        kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(0)\n", BLOCK_GPT_PARTITION_MAGIC_LBA);


    return (memcmp(efi, BLOCK_GPT_PARTITION_MAGIC, sizeof(efi)) == 0);
}

static bool detect_mbr_partition_table(device_t *device) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);


    uint16_t sig = 0;

    if (block_read(device, &sig, BLOCK_MBR_PARTITION_MAGIC_OFFSET, sizeof(sig)) <= 0)
        kpanicf("device::block: ERROR! Read Error at offset lba(0) offset(%d)\n", BLOCK_MBR_PARTITION_MAGIC_OFFSET);


    return (sig == BLOCK_MBR_PARTITION_MAGIC);
}


static void device_mkpart(device_t *device, inode_t *inode, void (*mkdev)(device_t *, mode_t), size_t index, size_t first, size_t end) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(mkdev);
    DEBUG_ASSERT(end > first);
    DEBUG_ASSERT(index < 1000);


    device_t *d = (device_t *)kcalloc(sizeof(device_t), 1, GFP_KERNEL);


    d->type = DEVICE_TYPE_BLOCK;

    strncpy(d->name, device->name, DEVICE_MAXNAMELEN);
    strncpy(d->description, device->description, DEVICE_MAXDESCLEN);


    if (index > 99) {

        d->name[strlen(device->name) + 0] = '1' + (index / 100);
        d->name[strlen(device->name) + 1] = '0' + (index % 100) / 10;
        d->name[strlen(device->name) + 2] = '0' + (index % 10);
        d->name[strlen(device->name) + 3] = '\0';

    } else if (index > 9) {

        d->name[strlen(device->name) + 0] = '1' + (index / 10);
        d->name[strlen(device->name) + 1] = '0' + (index % 10);
        d->name[strlen(device->name) + 2] = '\0';

    } else {

        d->name[strlen(device->name) + 0] = '1' + index;
        d->name[strlen(device->name) + 1] = '\0';
    }


    d->major = device->major;
    d->minor = device->minor + index + 1;

    d->init  = NULL;
    d->dnit  = NULL;
    d->reset = NULL;


    d->blk.read     = device->blk.read;
    d->blk.write    = device->blk.write;
    d->blk.blkmax   = device->blk.blkmax;
    d->blk.blksize  = device->blk.blksize;
    d->blk.blkoff   = first;
    d->blk.blkcount = end - first + 1;

    d->userdata = device->userdata;



    mode_t mode;

    struct stat st;
    if (vfs_getattr(inode, &st) < 0) {
        mode = 0666;
    } else {
        mode = st.st_mode;
    }

    mkdev(d, mode & 0777);
}



void block_parse_partitions(device_t *device, inode_t *inode, void (*mkdev)(device_t *, mode_t)) {

    DEBUG_ASSERT(device);
    DEBUG_ASSERT(device->blk.blksize);
    DEBUG_ASSERT(device->blk.blkcount);
    DEBUG_ASSERT(inode);
    DEBUG_ASSERT(mkdev);



    if (detect_gpt_partition_table(device)) {


#if DEBUG_LEVEL_TRACE
        kprintf("device::block: found GPT Partition Table in /dev/%s\n", device->name);
#endif


        // * Read GPT Header

        block_gpt_partition_t gpt = {0};

        if (block_read(device, &gpt, BLOCK_GPT_PARTITION_MAGIC_LBA * device->blk.blksize, sizeof(gpt)) != sizeof(gpt))
            kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(0)\n", BLOCK_GPT_PARTITION_MAGIC_LBA);



        // * Read GPT Entries

        for (size_t i = 0; i < gpt.table_entries; i++) {


            block_gpt_partition_entry_t entry = {0};

            if (block_read(device, &entry, (gpt.lba_table * device->blk.blksize) + (i * gpt.table_entry_size), sizeof(entry)) != sizeof(entry))
                kpanicf("device::block: ERROR! Read Error at lba(%ld) offset(%ld)\n", gpt.lba_table * device->blk.blksize, i * gpt.table_entry_size);


            if (entry.type_guid[0] == 0)
                continue;

            if (entry.type_guid[0] == 0xFF)
                continue;


#if DEBUG_LEVEL_TRACE
            kprintf("device::block: found GPT Partition %d in /dev/%s with GUID %.16phC\n", i, device->name, &entry.unique_guid);
#endif

            device_mkpart(device, inode, mkdev, i, entry.lba_first, entry.lba_last);
        }


    } else if (detect_mbr_partition_table(device)) {


#if DEBUG_LEVEL_TRACE
        kprintf("device::block: found MBR Partition Table in /dev/%s\n", device->name);
#endif


        // * Read MBR

        block_dos_partition_t part[4] = {0};

        if (block_read(device, &part, BLOCK_MBR_PARTITION_TABLE_OFFSET, sizeof(part)) != sizeof(part))
            kpanicf("device::block: ERROR! Read Error at offset lba(0) offset(0x1BE)\n");



        // * Read Partitions

        for (size_t i = 0; i < BLOCK_MBR_PARTITION_TABLE_ENTRIES; i++) {

            if (part[i].type == BLOCK_MBR_PARTITION_TYPE_EMPTY)
                continue;


#if DEBUG_LEVEL_TRACE
            kprintf("device::block: found MBR Partition %d in /dev/%s of type %X\n", i, device->name, part[i].type);
#endif

            device_mkpart(device, inode, mkdev, i, part[i].lba_start, part[i].lba_start + part[i].lba_blocks - 1);
        }


    } else {

#if DEBUG_LEVEL_WARN
        kprintf("device::block: WARN! unknown partition table for /dev/%s\n", device->name);
#endif
    }
}
