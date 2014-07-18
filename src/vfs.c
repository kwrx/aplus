//
//  vfs.c
//
//  Author:
//       Antonio Natale <inferdevil97@gmail.com>
//
//  Copyright (c) 2014 WareX
//
//  This program is kfree software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the kfree Software Foundation, either version 3 of the License, or
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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <aplus.h>
#include <aplus/task.h>
#include <aplus/spinlock.h>


volatile inode_t* fs_root;


static fsys_t* find_fsys(char* name) {
	extern uint32_t fsys_start;
	extern uint32_t fsys_end;
	
	fsys_t* fs = (fsys_t*) &fsys_start;
	uint32_t fsys_count = ((uint32_t) &fsys_end - (uint32_t) &fsys_start) / sizeof(fsys_t);
	
	for(int i = 0; i < fsys_count; i++) {
		if(strcmp(fs[i].name, name) == 0)
			return &fs[i];
	}
	
	return 0;
}


int fs_ioctl(struct inode* ino, int c, void* buf) {
	if(ino->ioctl)
		return ino->ioctl(ino, c, buf);
		
	errno = -ENOSYS;
	return -1;
}

int fs_read(struct inode* ino, uint32_t size, void* buf) {
	if(ino->read)
		return ino->read(ino, size, buf);
		
	errno = -ENOSYS;
	return 0;
}

int fs_write(struct inode* ino, uint32_t size, void* buf) {
	if(ino->write)
		return ino->write(ino, size, buf);
		
	errno = -ENOSYS;
	return 0;
}

int fs_trunc(struct inode* ino) {
	if(ino->trunc)
		return ino->trunc(ino);
		
	errno = -ENOSYS;
	return -1;
}

int fs_allocate(struct inode* ino, uint32_t size) {
	if(ino->allocate)
		return ino->allocate(ino, size);
		
	errno = -ENOSYS;
	return -1;
}

struct dirent* fs_readdir(struct inode* ino) {
	if(ino->readdir)
		return ino->readdir(ino);
		
	errno = -ENOSYS;
	return 0;
}

struct inode* fs_finddir(struct inode* ino, char* name) {
	if(ino->finddir)
		return ino->finddir(ino, name);
		
	errno = -ENOSYS;
	return 0;
}


struct inode* fs_creat(struct inode* ino, char* name, int mode) {
	if(ino->creat)
		return ino->creat(ino, name, mode);
		
	errno = -ENOSYS;
	return 0;
}

int fs_remove(struct inode* ino, char* name) {
	if(ino->remove)
		return ino->remove(ino, name);
		
	errno = -ENOSYS;
	return 0;
}

int fs_destroy(struct inode* ino) {
	if(ino->destroy)
		return ino->destroy(ino);
		
	errno = -ENOSYS;
	return 0;
}

uint32_t fs_geninode(char* name) {
	static int next_inode = 0;
	return next_inode++;
}

int fs_chroot(struct inode* ino) {
	if(!ino) {
		errno = -EINVAL;
		return -1;
	}
	
	fs_root = ino;
	return 0;
}

int fs_mount(struct inode* dev, struct inode* ino, int flags, char* fs) {
	if(!fs || !ino) {
		errno = -EINVAL;
		return -1;
	}

	fsys_t* f = find_fsys(fs);
	if(!f) {
		errno = -EINVAL;
		return -1;
	}
	
	ino->mask |= S_IFMT;
	ino->dev = dev;
	
	if(f->mount(dev, ino, flags) != 0)
		return -1;
		
	return 0;
}

int fs_umount(struct inode* ino) {
	ino->readdir = 0;
	ino->finddir = 0;
	ino->creat = 0;
	ino->remove = 0;
	ino->destroy = 0;
	ino->dev = 0;
	
	ino->mask &= ~S_IFMT;
}

int vfs_init() {
	fs_root = kmalloc(sizeof(inode_t));
	memset(fs_root, 0, sizeof(inode_t));

	strcpy(fs_root->name, "/");
	fs_root->inode = fs_geninode(fs_root->name);
	fs_root->uid = fs_root->gid = fs_root->length = fs_root->position = 0;
	fs_root->mask = S_IFDIR;
	
	fs_root->ioctl = 0;
	fs_root->read = 0;
	fs_root->write = 0;
	fs_root->trunc = 0;
	fs_root->allocate = 0;
	fs_root->readdir = 0;
	fs_root->finddir = 0;
	fs_root->creat = 0;
	fs_root->remove = 0;
	fs_root->destroy = 0;
	
	fs_root->ctime = time(NULL);
	fs_root->mtime = time(NULL);
	fs_root->atime = time(NULL);
	
	fs_root->disk_ptr = 0;
	fs_root->links_count = 0;
	fs_root->flock = 0;
	
	memset(fs_root->reserved, 0, INODE_RESERVED_SIZE);
	
	fs_root->parent = 0;
	fs_root->link = 0;
	fs_root->dev = 0;

	return 0;
}