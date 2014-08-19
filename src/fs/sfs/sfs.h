
//
//  sfs.h
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef _SFS_H
#define _SFS_H

#include <stdint.h>


#define SFS_MAGIC	0x534653
#define SFS_ENTRY_VOLUME		0x01
#define SFS_ENTRY_MARKER		0x02
#define SFS_ENTRY_UNUSED		0x10
#define SFS_ENTRY_DIRECTORY		0x11
#define SFS_ENTRY_FILE			0x12
#define SFS_ENTRY_UNUSABLE		0x18
#define SFS_ENTRY_RDIRECTORY	0x19
#define SFS_ENTRY_RFILE			0x1A

typedef uint64_t sfs_timestamp_t;




typedef struct sfs_superblock {
	char reserved0[11];
	char reserved1[21];
	char reserved2[372];
	sfs_timestamp_t timestamp;
	uint64_t size_data;
	uint64_t size_index;
	char magic[3];
	uint8_t version;
	uint64_t total_blocks;
	uint32_t reserved_blocks;
	uint8_t block_size;
	uint8_t checksum;
	char reserved3[64];
	char reserved4[2];
} __attribute__((packed)) sfs_superblock_t;



typedef struct sfs_inode {
	uint8_t type;
	char reserved[63];
} __attribute__((packed)) sfs_inode_t;


typedef struct sfs_inode_volume {
	uint8_t entry;
	char reserved[3];
	sfs_timestamp_t timestamp;
	char name[52];
} __attribute__((packed)) sfs_inode_volume_t;


typedef struct sfs_inode_marker {
	uint8_t type;
	char reserved[63];
} __attribute__((packed)) sfs_inode_marker_t;

typedef struct sfs_inode_index {
	uint8_t type;
	char reserved[63];
} __attribute__((packed)) sfs_inode_index_t;

typedef struct sfs_inode_unused {
	uint8_t type;
	char reserved[63];
} __attribute__((packed)) sfs_inode_unused_t;

typedef struct sfs_inode_directory {
	uint8_t type;
	uint8_t cont_counts;
	sfs_timestamp_t timestamp;
	char name[54];
} __attribute__((packed)) sfs_inode_directory_t;

typedef struct sfs_inode_file {
	uint8_t type;
	uint8_t cont_counts;
	sfs_timestamp_t timestamp;
	uint64_t block_start;
	uint64_t block_end;
	uint64_t length;
	char name[30];
} __attribute__((packed)) sfs_inode_file_t;

typedef struct sfs_inode_unusable {
	uint8_t type;
	char reserved0[9];
	uint64_t block_start;
	uint64_t block_end;
	char reserved1[38];
} __attribute__((packed)) sfs_inode_unusable_t;

typedef struct sfs_inode_rdirectory {
	uint8_t type;
	uint8_t cont_counts;
	sfs_timestamp_t timestamp;
	char name[54];
} __attribute__((packed)) sfs_inode_rdirectory_t;

typedef struct sfs_inode_rfile {
	uint8_t type;
	uint8_t cont_counts;
	sfs_timestamp_t timestamp;
	uint64_t block_start;
	uint64_t block_end;
	uint64_t length;
	char name[30];
} __attribute__((packed)) sfs_inode_rfile_t;


int sfs_check(sfs_superblock_t* sblock);


#endif
