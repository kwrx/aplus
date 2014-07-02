//
//  vfs.h
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

#ifndef _VFS_H
#define _VFS_H

#include <dirent.h>
#include <stdint.h>
#include <aplus/spinlock.h>

#define INODE_RESERVED_SIZE		(5 * sizeof(void*))
#define INODE_MAX_LINKS			512

typedef struct inode {
	char name[256];
	uint32_t inode, mask, uid, gid, length, position;
	
	int (*ioctl) (struct inode*, int, void*);
	int (*read) (struct inode*, uint32_t, void*);
	int (*write) (struct inode*, uint32_t, void*);
	int (*trunc) (struct inode*);
	int (*allocate) (struct inode*, uint32_t);
	int (*destroy) (struct inode*);
	
	/* Directory */
	struct dirent* (*readdir) (struct inode*);
	struct inode* (*finddir) (struct inode*, char*);
	struct inode* (*creat) (struct inode*, char*, int);
	int (*remove) (struct inode*, char*);
	
	
	uint32_t ctime;			/* 	Change 	*/
	uint32_t mtime;			/* 	Mod 	*/
	uint32_t atime;			/* 	Access	*/
	
	uint32_t disk_ptr;
	uint64_t links_count;

	spinlock_t flock;
	
	void* reserved[5];
	
	struct inode* parent;
	struct inode* link;
	struct inode* dev;
} inode_t;


typedef struct fsys {
	char name[256];
	
	int (*mount) (struct inode*, struct inode*, int);
	char padding[0x1C];
} fsys_t;

#define FSYS_DECLARE(name)																				\
	fsys_t fsys_##name __attribute__((section(".fsys"))) = {											\
		#name, name##_mount																				\
	}
#endif