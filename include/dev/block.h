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

#ifndef _DEV_BLOCK_H
#define _DEV_BLOCK_H

#ifndef __ASSEMBLY__


    #include <aplus.h>
    #include <aplus/debug.h>
    #include <aplus/syscall.h>
    #include <stdint.h>

    #include <dev/interface.h>


    #define BLOCK_MBR_PARTITION_MAGIC         0xAA55
    #define BLOCK_MBR_PARTITION_MAGIC_OFFSET  510
    #define BLOCK_MBR_PARTITION_TABLE_OFFSET  446
    #define BLOCK_MBR_PARTITION_TABLE_ENTRIES 4

    #define BLOCK_MBR_PARTITION_TYPE_EMPTY    0x00
    #define BLOCK_MBR_PARTITION_TYPE_EXTENDED 0x05


    #define BLOCK_GPT_PARTITION_MAGIC     "EFI PART"
    #define BLOCK_GPT_PARTITION_MAGIC_LBA 1



__BEGIN_DECLS


typedef struct block_gpt_partition {

    uint8_t sig[8];

    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;

    uint64_t lba_current;
    uint64_t lba_backup;
    uint64_t lba_first;
    uint64_t lba_last;

    uint8_t guid[16];

    uint64_t lba_table;
    uint32_t table_entries;
    uint32_t table_entry_size;
    uint32_t table_crc32;

} __packed block_gpt_partition_t;


typedef struct block_gpt_partition_entry {

    uint8_t type_guid[16];
    uint8_t unique_guid[16];

    uint64_t lba_first;
    uint64_t lba_last;

    uint64_t attributes;
    uint16_t name[36];

} __packed block_gpt_partition_entry_t;



typedef struct block_dos_partition {

    uint8_t attributes;

    struct {
        uint8_t h;
        uint16_t s : 6;
        uint16_t c : 10;
    } chs_start;

    uint8_t type;
    struct {
        uint8_t h;
        uint16_t s : 6;
        uint16_t c : 10;
    } chs_end;

    uint32_t lba_start;
    uint32_t lba_blocks;

} __packed block_dos_partition_t;


int block_getattr(device_t*, struct stat*);
ssize_t block_write(device_t*, const void*, off_t, size_t);
ssize_t block_read(device_t*, void*, off_t, size_t);
void block_init(device_t*);
void block_dnit(device_t*);
void block_parse_partitions(device_t*, inode_t*, void (*mkdev)(device_t*, mode_t));

__END_DECLS

#endif
#endif
