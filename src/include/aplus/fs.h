//
//  fs.h
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

#ifndef _FS_H
#define _FS_H


#define UID_ROOT		0
#define GID_ROOT		0


#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/dirent.h>

typedef struct inode {
	char name[255];

	dev_t dev;
	ino_t ino;
	mode_t mode;
	nlink_t nlink;
	uid_t uid;
	gid_t gid;
	dev_t rdev;
	off_t size;
	off_t position;
	
	time_t atime;
	time_t mtime;
	time_t ctime;
	
	void* userdata;

	int (*read) (struct inode* inode, char* ptr, int len);
	int (*write) (struct inode* inode, char* ptr, int len);
	
	struct dirent* (*readdir) (struct inode* inode, int index);
	struct inode* (*finddir) (struct inode* inode, char* name);
	struct inode* (*creat) (struct inode* inode, char* name, mode_t mode);
	
	int (*rename) (struct inode* inode, char* oldname, char* newname);
	int (*unlink) (struct inode* inode, char* name);
	int (*chown) (struct inode* inode, uid_t owner, gid_t group);
	void (*flush) (struct inode* inode);
	
	int (*ioctl) (struct inode* inode, int req, void* buf);


	struct inode* parent;
	struct inode* link;
} inode_t;


typedef struct fs {
	char name[255];
	int (*mount) (struct inode* idev, struct inode* idir, int flags);
} fs_t;

#define FS(name, mount)														\
	fs_t fs_##name = {														\
		#name, mount														\
	}; ATTRIBUTE("fs", fs_##name)




#endif
