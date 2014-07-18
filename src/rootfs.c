//
//  rootfs.c
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


#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/task.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

extern inode_t* fs_root;


typedef struct rootfs_inode {
	inode_t* inode;
	
	struct rootfs_inode* next;
	struct rootfs_inode* prev;
} rootfs_inode_t;

rootfs_inode_t* rootfs_queue = 0;


struct dirent* rootfs_readdir(struct inode* ino) {
	if(!ino)
		return 0;
				
	uint32_t index = ino->position;
	uint32_t i = 0;
	
	rootfs_inode_t* tmp = rootfs_queue;
	
	while(1) {
		if(!tmp)
			return 0;
			
		if(!tmp->inode)
			return 0;
			
		if(tmp->inode->parent == ino)
			i++;
			
		if(i == index + 1)
			break;
			
		tmp = tmp->next;
	}
	
	struct dirent* ent = kmalloc(sizeof(struct dirent));
	memset(ent, 0, sizeof(struct dirent));
	
	strcpy(ent->d_name, tmp->inode->name);
	ent->d_ino = tmp->inode->inode;
	
	ino->position += 1;
	
	return ent;
}

struct inode* rootfs_finddir(struct inode* ino, char* name) {
	if(!ino)
		return 0;
	
	if(!name)
		return 0;
	
	if(name[0] == 0)
		return 0;
		
	if(strcmp(name, ".") == 0)
		return ino;
		
	if(strcmp(name, "..") == 0)
		return ino->parent;
		
	rootfs_inode_t* tmp = rootfs_queue;
	while(tmp) {
		inode_t* i = tmp->inode;
		if(!i)
			return 0;
			
		if(i->parent == ino)
			if(strcmp(i->name, name) == 0)
				return i;
				
		tmp = tmp->next;
	}
	
	return 0;
}

int rootfs_remove(struct inode* ino, char* name) {
	if(!ino)
		return -1;
	
	if(!name)
		return -1;
	
	if(name[0] == 0)
		return -1;
		
	if(strcmp(name, ".") == 0)
		return -1;
		
	if(strcmp(name, "..") == 0)
		return -1;
		
	rootfs_inode_t* tmp = rootfs_queue;
	while(tmp) {
		inode_t* i = tmp->inode;
		if(!i)
			return -1;
			
		if(i->parent == ino) {
			if(strcmp(i->name, name) == 0) {
				if(tmp->prev)
					tmp->prev->next = tmp->next;
					
				if(tmp->next)
					tmp->next->prev = tmp->prev;
					
				fs_destroy(tmp->inode);
						
				tmp->prev = 0;
				tmp->next = 0;
				tmp->inode = 0;
				kfree(tmp);
				
				return 0;
			}
		}
				
		tmp = tmp->next;
	}
	
	return -1;
}

struct inode* rootfs_creat(struct inode* ino, char* name, int mode) {
	if(!ino)
		return 0;
	
	if(!name)
		return 0;
	
	if(name[0] == 0)
		return 0;

	inode_t* f = kmalloc(sizeof(inode_t));
	memset(f, 0, sizeof(inode_t));
	
	f->parent = ino;
	strcpy(f->name, name);
	f->inode = fs_geninode(f->name);
	f->uid = f->gid = f->length = f->position = 0;
	f->mask = mode;
	
	f->ioctl = 0;
	f->read = 0;
	f->write = 0;
	f->trunc = 0;
	f->allocate = 0;
	f->destroy = 0;
	
	if(mode & S_IFDIR) {
		f->readdir = rootfs_readdir;
		f->finddir = rootfs_finddir;
		f->creat = rootfs_creat;
		f->remove = rootfs_remove;
	} else {
		f->readdir = 0;
		f->finddir = 0;
		f->creat = 0;
		f->remove = 0;
	}
	
	f->ctime = time(NULL);
	f->mtime = time(NULL);
	f->atime = time(NULL);
	
	f->disk_ptr = 0;
	f->links_count = 0;
	f->flock = 0;
	
	memset(f->reserved, 0, INODE_RESERVED_SIZE);
	

	f->link = 0;
	f->dev = fs_root;
	
	
	rootfs_inode_t* rfs = kmalloc(sizeof(rootfs_inode_t));
	rfs->inode = f;
	rfs->next = 0;
	rfs->prev = 0;

	
	if(rootfs_queue) {
	
		rootfs_inode_t* tmp = rootfs_queue;
	
		while(tmp->next)
			tmp = tmp->next;
			
		rfs->prev = tmp;
		tmp->next = rfs;
	} else {
		rootfs_queue = rfs;
	}
		
		
	return f;
}



int rootfs_init() {
	
	rootfs_queue = 0;
	
	fs_root->readdir = rootfs_readdir;
	fs_root->finddir = rootfs_finddir;
	fs_root->creat = rootfs_creat;
	fs_root->remove = rootfs_remove;
	
	int fd = open("/dev", O_CREAT | O_EXCL, S_IFDIR);
	if(fd < 0) {
		video_puts("rootfs: cannot create \"/dev\" directory\n");
		return -1;
	}
	
	close(fd);
	
	return 0;
}
