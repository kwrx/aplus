//
//  procfs.c
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
#include <fcntl.h>


typedef struct procfs_inode {
	inode_t* inode;
	
	struct procfs_inode* next;
	struct procfs_inode* prev;
} procfs_inode_t;

extern task_t* current_task;
extern task_t* kernel_task;


static task_t* procfs_get_task(int index) {
	task_t* tmp = current_task;
	if(!tmp)
		return 0;
		
	while(tmp->prev)
		tmp = tmp->prev;
		
	int i = 0;
	while(i < index) {
		if(tmp->next)
			tmp = tmp->next;
		else
			return 0;
			
		i++;
	}

	return tmp;
}

static int procfs_get_taskcount() {
	int i = 0;
	while(procfs_get_task(i))
		i++;
		
	return i;
}



int procfs_add_inode(inode_t* ino, inode_t* f) {
	procfs_inode_t* pfs = kmalloc(sizeof(procfs_inode_t));
	pfs->inode = f;
	pfs->next = 0;
	pfs->prev = 0;
	
	procfs_inode_t* queue = ino->dev->disk_ptr;
	if(queue) {
		procfs_inode_t* tmp = queue;
		
		while(tmp->next)
			tmp = tmp->next;
			
		pfs->prev = tmp;
		tmp->next = pfs;
	} else {
		ino->dev->disk_ptr = pfs;
	}
	
	return 0;
}



struct dirent* procfs_readdir(struct inode* ino) {
	if(!ino)
		return 0;
		
	if(!ino->dev)
		return 0;
		
	uint32_t index = ino->position;
	uint32_t i = 0;
	char* name = 0;
	int inode = 0;
	
	if(procfs_get_task(index) != 0 && ino == ino->dev) {
		task_t* t = procfs_get_task(index);
		
		name = kmalloc(16);
		memset(name, 0, 16);
		
		sprintf(name, "%d", t->pid);
		
		inode = t->pid;
	} else {
		procfs_inode_t* tmp = ino->dev->disk_ptr;
		
		if(ino == ino->dev)
			index -= procfs_get_taskcount();
		
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
		
		name = tmp->inode->name;
		inode = tmp->inode->inode;
	}
	
	struct dirent* ent = kmalloc(sizeof(struct dirent));
	memset(ent, 0, sizeof(struct dirent));
	
	strcpy(ent->d_name, name);
	ent->d_ino = inode;
		
	ino->position += 1;
	return ent;
}


struct inode* procfs_finddir(struct inode* ino, char* name) {
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
	
	if(!ino->dev)
		return 0;
		
		
	if(ino == ino->dev) {
		task_t* t = 0;
		int c = 0;
		while(t = procfs_get_task(c++)) {
			static char pbuf[16];
			memset(pbuf, 0, 16);
			
			sprintf(pbuf, "%d", t->pid);
			if(strcmp(pbuf, name) == 0)
				return procfs_task_create(ino, t);
		}
	}
		
	procfs_inode_t* tmp = ino->dev->disk_ptr;
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

int procfs_remove(struct inode* ino, char* name) {
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
		
	if(!ino->dev)
		return -1;
		
	procfs_inode_t* tmp = ino->dev->disk_ptr;
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
					
				fs_destroy(i);
				
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



struct inode* procfs_creat(struct inode* ino, char* name, int mode) {
	if(!ino)
		return 0;
	
	if(!name)
		return 0;
	
	if(name[0] == 0)
		return 0;
		
	if(strcmp(name, ".") == 0)
		return 0;
		
	if(strcmp(name, "..") == 0)
		return 0;
	
	if(!ino->dev)
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
		f->readdir = procfs_readdir;
		f->finddir = procfs_finddir;
		f->creat = procfs_creat;
		f->remove = procfs_remove;
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
	f->dev = ino->dev;
	
	procfs_add_inode(ino, f);

	return f;
}


int procfs_destroy(struct inode* ino) {
	if(!ino)
		return 0;
		
	procfs_inode_t* tmp = ino->dev->disk_ptr;
	while(tmp) {
		inode_t* i = tmp->inode;
		if(!i)
			return -1;
			
		if(i->parent == ino)
			fs_remove(ino, i->name);
		
		tmp = tmp->next;
	}
	
	return 0;
}
