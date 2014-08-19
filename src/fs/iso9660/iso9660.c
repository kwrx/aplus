
//
//  iso9660.c
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

#include <stdint.h>

#include "iso9660.h"

typedef struct iso9660_inode {
	struct inode* inode;
	
	struct iso9660_inode* next;
	struct iso9660_inode* prev;
} iso9660_inode_t;



struct inode* iso9660_check_inode(struct inode* parent, char* name) {
	iso9660_inode_t* tmp = parent->dev->reserved[0];
	if(!tmp)
		return NULL;
		
	while(tmp) {
		if(tmp->inode->parent == parent)
			if(strcmp(tmp->inode->name, name) == 0)
				return tmp->inode;
				
		tmp = tmp->next;
	}
	
	return NULL;
}

void iso9660_add_inode(struct inode* ino) {
	iso9660_inode_t* tmp = ino->dev->reserved[0];
	if(!tmp) {
		tmp = kmalloc(sizeof(iso9660_inode_t));
		tmp->inode = ino;
		tmp->next = 0;
		tmp->prev = 0;
		
		ino->dev->reserved[0] = tmp;
		return;
	}
		
	while(tmp->next)
		tmp = tmp->next;
		
	iso9660_inode_t* n = kmalloc(sizeof(iso9660_inode_t));
	n->inode = ino;
	n->next = 0;
	n->prev = tmp;
	
	tmp->next = n;
}

void iso9660_del_inode(struct inode* ino) {
	iso9660_inode_t* tmp = ino->dev->reserved[0];
	if(!tmp)
		return;
		
	while(tmp) {
		if(tmp->inode == ino) {
			if(tmp->next)
				tmp->next->prev = tmp->prev;
				
			if(tmp->prev)
				tmp->prev->next = tmp->next;
				
			kfree(tmp);
			break;
		}
				
		tmp = tmp->next;
	}
}



uint32_t iso9660_getroot(inode_t* dev) {
	iso9660_pvd_t* pvd = kmalloc(ISO9660_VOLDESC_SIZE);
	memset(pvd, 0, ISO9660_VOLDESC_SIZE);
	
	dev->position = ISO9660_PVD;
	if(fs_read(dev, ISO9660_VOLDESC_SIZE, pvd) != ISO9660_VOLDESC_SIZE) {
		kfree(pvd);
		return 0;
	}
	
	return (uint32_t) pvd->rootdir;
}

void iso9660_checkname(char* name) {
	char* p = strchr(name, ';');
	if(p) {
		*p-- = 0;
		if(*p == '.')
			*p = 0;
	}
		
	for(int i = 0; i < strlen(name); i++)
		name[i] = tolower(name[i]);
}

uint16_t iso9660_getmsb16(uint32_t val) {
	return (val >> 16) & 0xFFFF;
}

uint32_t iso9660_getmsb32(uint64_t val) {
	return (val >> 32) & 0xFFFFFFFF;
}

uint16_t iso9660_getlsb16(uint32_t val) {
	return (val >> 0) & 0xFFFF;
}

uint32_t iso9660_getlsb32(uint64_t val) {
	return (val >> 0) & 0xFFFFFFFF;
}


