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


#ifndef _ISO9660_H
#define _ISO9660_H

#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>

#include <aplus/utils/list.h>


#define ISO9660_BLOCK_SIZE      2048
#define ISO9660_MAX_NAME        255
#define ISO9660_MAX_SYMLINK     CONFIG_PATH_MAX


#define ISO9660_VOLUME_DESCRIPTOR_TYPE_BOOT_RECORD              0
#define ISO9660_VOLUME_DESCRIPTOR_TYPE_PRIMARY                  1
#define ISO9660_VOLUME_DESCRIPTOR_TYPE_SUPPLEMENTARY            2
#define ISO9660_VOLUME_DESCRIPTOR_TYPE_PARTITION                3
#define ISO9660_VOLUME_DESCRIPTOR_TYPE_TERMINATOR               255


#define ISO9660_ROCKRIDGE_ENTRY_PX                              "PX"
#define ISO9660_ROCKRIDGE_ENTRY_PN                              "PN"
#define ISO9660_ROCKRIDGE_ENTRY_SL                              "SL"
#define ISO9660_ROCKRIDGE_ENTRY_NM                              "NM"
#define ISO9660_ROCKRIDGE_ENTRY_CL                              "CL"
#define ISO9660_ROCKRIDGE_ENTRY_PL                              "PL"
#define ISO9660_ROCKRIDGE_ENTRY_RE                              "RE"
#define ISO9660_ROCKRIDGE_ENTRY_TF                              "TF"
#define ISO9660_ROCKRIDGE_ENTRY_SF                              "SF"


typedef struct iso9660_volume_descriptor {

    uint8_t type;
    uint8_t id[5];
    uint8_t version;


    union {

        struct {

            uint8_t boot_system_id[32];
            uint8_t boot_id[32];
            uint8_t boot_system_use[1977];

        } __packed boot_record;


        struct {

            uint8_t __unused[1];
            uint8_t system_id[32];
            uint8_t volume_id[32];
            uint8_t __unused2[8];
            
            struct {
                uint32_t lsb;
                uint32_t msb;
            } __packed volume_space_size;

            uint8_t __unused3[32];

            struct {
                uint16_t lsb;
                uint16_t msb;
            } __packed volume_set_size;

            struct {
                uint16_t lsb;
                uint16_t msb;
            } __packed volume_sequence_number;

            struct {
                uint16_t lsb;
                uint16_t msb;
            } __packed logical_block_size;

            struct {
                uint32_t lsb;
                uint32_t msb;
            } __packed path_table_size;

            uint32_t type_l_path_table;
            uint32_t type_l_path_table_opt;

            uint32_t type_m_path_table;
            uint32_t type_m_path_table_opt;

            uint8_t root_directory_record[34];

            uint8_t volume_set_id[128];
            uint8_t publisher_id[128];
            uint8_t preparer_id[128];
            uint8_t application_id[128];
            uint8_t copyright_file_id[37];
            uint8_t abstract_file_id[37];
            uint8_t bibliographic_file_id[37];

            struct {
                uint8_t creation[17];
                uint8_t modification[17];
                uint8_t expiration[17];
                uint8_t effective[17];
            } __packed volume_creation_date;

            uint8_t file_structure_version;
            uint8_t __unused4[1];

            uint8_t application_data[512];
            uint8_t __unused5[653];

        } __packed primary;

    };

} __packed iso9660_volume_descriptor_t;


typedef struct iso9660_directory_record {

    uint8_t length;
    uint8_t extended_attribute_record_length;

    struct {
        uint32_t lsb;
        uint32_t msb;
    } __packed extent_location;

    struct {
        uint32_t lsb;
        uint32_t msb;
    } __packed data_length;

    struct {
        uint8_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
        uint8_t offset;
    } __packed recording_date_time;

    uint8_t file_flags;
    uint8_t file_unit_size;
    uint8_t interleave_gap_size;

    struct {
        uint16_t lsb;
        uint16_t msb;
    } __packed volume_sequence_number;

    uint8_t length_of_file_identifier;
    uint8_t file_identifier[0];

} __packed iso9660_directory_record_t;


typedef struct iso9660_inode {

    char name[ISO9660_MAX_NAME + 1];
    char symlink[ISO9660_MAX_SYMLINK + 1];

    struct iso9660_directory_record record;
    struct stat st;

} iso9660_inode_t;


typedef struct iso9660 {

    size_t blocks;
    size_t block_size;

    union {
        uint8_t __padding[64];
        iso9660_directory_record_t root;
    };

    inode_t* dev;
    inode_t* dir;

    ino_t next_ino;

} iso9660_t;


typedef struct iso9660_rockridge_entry {

    char name[2];
    uint8_t length;
    uint8_t version;
    
    union {

        struct {

            uint64_t mode;
            uint64_t nlinks;
            uint64_t uid;
            uint64_t gid;
            uint64_t ino;

        } __packed px;

        struct {

            uint64_t dev;
            uint64_t rdev;

        } __packed pn;

        struct {

            uint8_t flags;
            uint8_t data[0];

        } __packed sl;

        struct {

            uint8_t flags;

            struct {

                uint8_t flags;
                uint8_t length;
                uint8_t data[0];

            } __packed components[0];

        } __packed nm;

        struct {

            struct {
                uint32_t lsb;
                uint32_t msb;
            } __packed location;

        } __packed cl;

        struct {

            struct {
                uint32_t lsb;
                uint32_t msb;
            } __packed location;

        } __packed pl;

    };

} __packed iso9660_rockridge_entry_t;


int iso9660_getattr (inode_t*, struct stat*);

ssize_t iso9660_read (inode_t*, void*, off_t, size_t);
ssize_t iso9660_readlink (inode_t*, char*, size_t);
ssize_t iso9660_readdir (inode_t*, struct dirent*, off_t, size_t);
inode_t* iso9660_finddir (inode_t*, const char*);

iso9660_inode_t* iso9660_cache_load(cache_t* cache, iso9660_t* iso9660, size_t position);
iso9660_inode_t* iso9660_cache_sync(cache_t* cache, iso9660_t* iso9660, size_t position, iso9660_inode_t* inode);

#endif