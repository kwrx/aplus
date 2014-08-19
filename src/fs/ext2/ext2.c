//
//  ext2.c
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

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

#include <aplus.h>
#include <aplus/vfs.h>
#include <aplus/spinlock.h>

#include "ext2.h"


typedef struct {
	ext2_superblock_t* superblock;
	ext2_bgdescriptor_t* block_groups; 
	
	inode_t* root_node; 
	inode_t* block_device; 

	uint32_t block_size; 
	uint32_t pointers_per_block;
	uint32_t inodes_per_group; 
	uint32_t block_group_count; 

	ext2_disk_cache_entry_t* disk_cache; 
	uint32_t cache_entries; 
	uint32_t cache_time; 

	spinlock_t lock;
} ext2_fs_t;


#define BGDS 	(this->block_group_count)
#define SB 		(this->superblock)
#define BGD 	(this->block_groups)
#define RN 		(this->root_node)
#define DC 		(this->disk_cache)

#define BLOCKBIT(n) (bg_buffer[((n) >> 3)] & (1 << (((n) % 8))))
#define BLOCKBYTE(n) (bg_buffer[((n) >> 3)])
#define SETBIT(n) (1 << (((n) % 8)))


#define fs_write_ext(in, p, s, b)	\
	in->position = p;				\
	fs_write(in, s, b);
	
#define fs_read_ext(in, p, s, b) 	\
	in->position = p;				\
	fs_read(in, s, b);


static uint32_t node_from_file(ext2_fs_t* this, ext2_inodetable_t* inode, ext2_dir_t* direntry, inode_t* fnode);
static uint32_t ext2_root(ext2_fs_t* this, ext2_inodetable_t* inode, inode_t* fnode);
static ext2_inodetable_t * read_inode(ext2_fs_t* this, uint32_t inode);


static uint32_t get_cache_time(ext2_fs_t* this) {
	return this->cache_time++;
}

static int cache_flush_dirty(ext2_fs_t* this, uint32_t ent_no) {
	fs_write_ext(this->block_device, (DC[ent_no].block_no) * this->block_size, this->block_size, (uint8_t *)(DC[ent_no].block));
	
	DC[ent_no].dirty = 0;
	return 0;
}


static int read_block(ext2_fs_t* this, uint32_t block_no, uint8_t* buf) {
	if(!block_no)
		return -1;
		
	spinlock_lock(&this->lock);
	
	if(!DC) {
		fs_read_ext(this->block_device, block_no * this->block_size, this->block_size, (uint8_t*) buf);
		spinlock_unlock(&this->lock);
		return 0;
	}
	
	int oldest = -1;
	uint32_t oldest_age = UINT32_MAX;
	
	for(uint32_t i = 0; i < this->cache_entries; ++i) {
		if(DC[i].block_no == block_no) {
			DC[i].last_use = get_cache_time(this);
			
			memcpy(buf, DC[i].block, this->block_size);
			
			spinlock_unlock(&this->lock);
			return 0;
		}
		
		if(DC[i].last_use < oldest_age) {
			oldest = i;
			oldest_age = DC[i].last_use;
		}
	}
	
	if(DC[oldest].dirty)
		cache_flush_dirty(this, oldest);
		
	fs_read_ext(this->block_device, block_no * this->block_size, this->block_size, (uint8_t *)DC[oldest].block);
	memcpy(buf, DC[oldest].block, this->block_size);
	
	DC[oldest].block_no = block_no;
	DC[oldest].last_use = get_cache_time(this);
	DC[oldest].dirty = 0;
	
	spinlock_unlock(&this->lock);
	return 0;
}

static int write_block(ext2_fs_t* this, uint32_t block_no, uint8_t* buf) {
	if(!block_no)
		return -1;
		
	spinlock_lock(&this->lock);
	
	int oldest = -1;
	uint32_t oldest_age = UINT32_MAX;
	for(uint32_t i = 0; i < this->cache_entries; ++i) {
		if(DC[i].block_no == block_no) {
			DC[i].last_use = get_cache_time(this);
			DC[i].dirty = 1;
			
			memcpy(DC[i].block, buf, this->block_size);
			
			spinlock_unlock(&this->lock);
			return 0;
		}
		
		
		if(DC[i].last_use < oldest_age) {
			oldest = i;
			oldest_age = DC[i].last_use;
		}
	}
	
	if(DC[oldest].dirty)
		cache_flush_dirty(this, oldest);
		
	memcpy(DC[oldest].block, buf, this->block_size);
	DC[oldest].block_no = block_no;
	DC[oldest].last_use = get_cache_time(this);
	DC[oldest].dirty = 1;
	
	spinlock_unlock(&this->lock);
	return 0;
}


static uint32_t set_block_number(ext2_fs_t* this, ext2_inodetable_t* inode, uint32_t iblock, uint32_t rblock) {
	uint32_t p = this->pointers_per_block;
	uint32_t a, b, c, d, e, f, g;
	
	if(iblock < EXT2_DIRECT_BLOCKS) {
		inode->block[iblock] = rblock;
		return 0;
	} else if(iblock < EXT2_DIRECT_BLOCKS + p) {
		uint8_t* tmp = kmalloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS], tmp);
		
		((uint32_t*) tmp)[iblock - EXT2_DIRECT_BLOCKS] = rblock;
		write_block(this, inode->block[EXT2_DIRECT_BLOCKS], tmp);
		
		return 0;
	} else if(iblock < EXT2_DIRECT_BLOCKS + p + p * p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b / p;
		d = b - c * p;
		
		uint8_t* tmp = kmalloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 1], tmp);
		
		uint32_t nblock = ((uint32_t*) tmp) [c];
		read_block(this, nblock, tmp);
		
		((uint32_t*) tmp) [d] = rblock;
		write_block(this, nblock, tmp);
		
		return 0;
	} else if(iblock < EXT2_DIRECT_BLOCKS + p + p * p + p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b - p * p;
		d = c / (p * p);
		e = c - d * p * p;
		f = e / p;
		g = e - f * p;
		
		uint8_t* tmp = kmalloc(this->block_size);
		read_block(this, inode->block[EXT2_DIRECT_BLOCKS + 2], tmp);
		
		uint32_t nblock = ((uint32_t*) tmp) [d];
		read_block(this, nblock, tmp);
		
		nblock = ((uint32_t*) tmp) [f];
		read_block(this, nblock, tmp);
		
		((uint32_t*) tmp) [g] = nblock;
		write_block(this, nblock, tmp);
		
		return 0;
	}
	
	return -1;
}






