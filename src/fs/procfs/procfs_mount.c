//
//  procfs_mount.c
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

#include <aplus/vfs.h>
#include <aplus/task.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

extern task_t* kernel_task;

extern struct dirent* procfs_readdir(struct inode* ino);
extern struct inode* procfs_finddir(struct inode* ino, char* name);
extern struct inode* procfs_creat(struct inode* ino, char* name, int mode);
extern int procfs_remove(struct inode* ino, char* name);
extern int procfs_destroy(struct inode* ino);

int procfs_mount(struct inode* dev, struct inode* ino, int flags) {

	if(!ino)
		return -1;
	
	ino->readdir = procfs_readdir;
	ino->finddir = procfs_finddir;
	ino->creat = procfs_creat;
	ino->remove = procfs_remove;
	ino->destroy = procfs_destroy;
	
	ino->dev = ino;
	
	
	procfs_add_inode(ino, procfs_cmdline_create(ino, kernel_task));
	procfs_add_inode(ino, procfs_environ_create(ino, kernel_task));
	procfs_add_inode(ino, procfs_meminfo_create(ino));
	procfs_add_inode(ino, procfs_filesystems_create(ino));
	

	return 0;
}

FSYS_DECLARE(procfs);