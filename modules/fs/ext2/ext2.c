#include <aplus.h>
#include <aplus/base.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/module.h>
#include <aplus/debug.h>
#include <libc.h>

#include "ext2.h"




#define EXT2_BGD_BLOCK 2

#define E_SUCCESS   0
#define E_BADBLOCK  1
#define E_NOSPACE   2
#define E_BADPARENT 3

#undef _symlink
#define _symlink(inode) ((char *)(inode)->block)



typedef struct {
    ext2_superblock_t* superblock;
    ext2_bgdescriptor_t* block_groups;
    
    inode_t* root_node;
    inode_t* block_device;

    uint32_t block_size;
    uint32_t pointers_per_block;
    uint32_t inodes_per_group;
    uint32_t block_group_count;

    spinlock_t lock;
    uint8_t bgd_block_span;
    uint8_t bgd_offset;

    uint32_t inode_size;
    uint8_t* cache_data;
    int flags;
} ext2_fs_t;



#define BGDS (ext2->block_group_count)
#define SB   (ext2->superblock)
#define BGD  (ext2->block_groups)
#define RN   (ext2->root_node)
#define DC   (ext2->disk_cache)


#define BLOCKBIT(n)  (bg_buffer[((n) >> 3)] & (1 << (((n) % 8))))
#define BLOCKBYTE(n) (bg_buffer[((n) >> 3)])
#define SETBIT(n)    (1 << (((n) % 8)))


static uint32_t node_from_file(ext2_fs_t * ext2, ext2_inodetable_t *inode, ext2_dir_t *direntry,  inode_t *fnode);
static uint32_t ext2_root(ext2_fs_t * ext2, ext2_inodetable_t *inode, inode_t *fnode);
static ext2_inodetable_t * read_inode(ext2_fs_t * ext2, uint32_t inode);
static void refresh_inode(ext2_fs_t * ext2, ext2_inodetable_t * inodet,  uint32_t inode);
static int write_inode(ext2_fs_t * ext2, ext2_inodetable_t *inode, uint32_t index);
static inode_t * finddir_ext2(inode_t *node, char *name);
static uint32_t allocate_block(ext2_fs_t * ext2);



static int rewrite_superblock(ext2_fs_t* ext2) {
    ext2->block_device->position = 1024;
	vfs_write(ext2->block_device, (void*) SB, sizeof(ext2_superblock_t));

	return E_OK;
}


static int read_block(ext2_fs_t* ext2, uint32_t block_no, uint8_t* buf) {
	if (!block_no) {
        kprintf(ERROR "ext2: invalid block number: %d\n", block_no);
		return E_ERR;
	}

	spinlock_lock(&ext2->lock);
	
    ext2->block_device->position = block_no * ext2->block_size;
    vfs_read(ext2->block_device, buf, ext2->block_size);
	
    spinlock_unlock(&ext2->lock);
	return E_OK;
}


static int write_block(ext2_fs_t* ext2, uint32_t block_no, uint8_t* buf) {
	if (!block_no) {
		kprintf(ERROR "ext2: attempted to write to block #0. Enable tracing and retry ext2 operation.\n");
		kprintf(ERROR "ext2: your file system is most likely corrupted now.\n");

		return E_ERR;
	}

	spinlock_lock(&ext2->lock);

    ext2->block_device->position = block_no * ext2->block_size;
	vfs_write(ext2->block_device, buf, ext2->block_size);
	
    spinlock_unlock(&ext2->lock);
    return E_OK;
}



static uint32_t set_block_number(ext2_fs_t* ext2, ext2_inodetable_t* inode, uint32_t inode_no, uint32_t iblock, uint32_t rblock) {

	uint32_t p = ext2->pointers_per_block;
	uint32_t a, b, c, d, e, f, g;
	uint8_t* tmp;

	if (iblock < EXT2_DIRECT_BLOCKS) {
		inode->block[iblock] = rblock;
		return E_SUCCESS;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p) {
		if (!inode->block[EXT2_DIRECT_BLOCKS]) {
			uint32_t block_no = allocate_block(ext2);

			if (!block_no) {
                kprintf(ERROR "ext2: no space left on device!\n");

                errno = ENOSPC;
                return E_ERR;
            }

			inode->block[EXT2_DIRECT_BLOCKS] = block_no;
			write_inode(ext2, inode, inode_no);
		}

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t*)tmp);

		((uint32_t *)tmp)[iblock - EXT2_DIRECT_BLOCKS] = rblock;
		write_block(ext2, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t*)tmp);

		kfree(tmp);
		return E_OK;

	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b / p;
		d = b - c * p;

		if (!inode->block[EXT2_DIRECT_BLOCKS+1]) {
			uint32_t block_no = allocate_block(ext2);
			
            if (!block_no) {
                kprintf(ERROR "ext2: no space left on device!\n");

                errno = ENOSPC;
                return E_ERR;
            }

			inode->block[EXT2_DIRECT_BLOCKS + 1] = block_no;
			write_inode(ext2, inode, inode_no);
		}

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t*)tmp);

		if (!((uint32_t*)tmp)[c]) {
			uint32_t block_no = allocate_block(ext2);

			if (!block_no) 
                goto no_space_free;

			((uint32_t*)tmp)[c] = block_no;
			write_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t*)tmp);
		}

		uint32_t nblock = ((uint32_t*)tmp)[c];
		read_block(ext2, nblock, (uint8_t*)tmp);

		((uint32_t  *)tmp)[d] = rblock;
		write_block(ext2, nblock, (uint8_t*)tmp);

		kfree(tmp);
		return E_OK;

	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p + p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b - p * p;
		d = c / (p * p);
		e = c - d * p * p;
		f = e / p;
		g = e - f * p;

		if (!inode->block[EXT2_DIRECT_BLOCKS + 2]) {
			uint32_t block_no = allocate_block(ext2);
			
            if (!block_no) {
                kprintf(ERROR "ext2: no space left on device!\n");

                errno = ENOSPC;
                return E_ERR;
            }

			inode->block[EXT2_DIRECT_BLOCKS+2] = block_no;
			write_inode(ext2, inode, inode_no);
		}

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t*)tmp);

		if (!((uint32_t *)tmp)[d]) {
			uint32_t block_no = allocate_block(ext2);

			if (!block_no) 
                goto no_space_free;

			((uint32_t *)tmp)[d] = block_no;
			write_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t*)tmp);
		}

		uint32_t nblock = ((uint32_t *)tmp)[d];
		read_block(ext2, nblock, (uint8_t*)tmp);

		if (!((uint32_t *)tmp)[f]) {
			uint32_t block_no = allocate_block(ext2);

			if (!block_no) 
                goto no_space_free;
            
			((uint32_t *)tmp)[f] = block_no;
			write_block(ext2, nblock, (uint8_t*)tmp);
		}

		nblock = ((uint32_t *)tmp)[f];
		read_block(ext2, nblock, (uint8_t*)tmp);

		((uint32_t *)tmp)[g] = nblock;
		write_block(ext2, nblock, (uint8_t*)tmp);

		kfree(tmp);
		return E_OK;
	}

	kprintf(ERROR "ext2: driver tried to write to a block number that was too high (%d)\n", rblock);
	return E_ERR;

no_space_free:
	kfree(tmp);

	kprintf(ERROR "ext2: no space left on device!\n");
    return E_ERR;
}


static uint32_t get_block_number(ext2_fs_t* ext2, ext2_inodetable_t* inode, uint32_t iblock) {

	uint32_t p = ext2->pointers_per_block;
	uint32_t a, b, c, d, e, f, g;
	uint8_t* tmp;

	if (iblock < EXT2_DIRECT_BLOCKS) {
		return inode->block[iblock];
	} else if (iblock < EXT2_DIRECT_BLOCKS + p) {

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS], (uint8_t*)tmp);

		uint32_t out = ((uint32_t *)tmp)[iblock - EXT2_DIRECT_BLOCKS];
		kfree(tmp);
		return out;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b / p;
		d = b - c * p;

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 1], (uint8_t*)tmp);

		uint32_t nblock = ((uint32_t *)tmp)[c];
		read_block(ext2, nblock, (uint8_t*)tmp);

		uint32_t out = ((uint32_t  *)tmp)[d];
		kfree(tmp);
		return out;
	} else if (iblock < EXT2_DIRECT_BLOCKS + p + p * p + p) {
		a = iblock - EXT2_DIRECT_BLOCKS;
		b = a - p;
		c = b - p * p;
		d = c / (p * p);
		e = c - d * p * p;
		f = e / p;
		g = e - f * p;

		tmp = kmalloc(ext2->block_size, GFP_KERNEL);
		read_block(ext2, inode->block[EXT2_DIRECT_BLOCKS + 2], (uint8_t*)tmp);

		uint32_t nblock = ((uint32_t *)tmp)[d];
		read_block(ext2, nblock, (uint8_t*)tmp);

		nblock = ((uint32_t *)tmp)[f];
		read_block(ext2, nblock, (uint8_t*)tmp);

		uint32_t out = ((uint32_t  *)tmp)[g];
		kfree(tmp);
		return out;
	}

	kprintf(ERROR "ext2: driver tried to read to a block number that was too high (%d)\n", iblock);
	return 0;
}

static int write_inode(ext2_fs_t* ext2, ext2_inodetable_t* inode, uint32_t index) {

	uint32_t group = index / ext2->inodes_per_group;
	if (group > BGDS) {
		kprintf(ERROR "ext2: invalid group number: %d\n", group);
		return E_ERR;
	}

	uint32_t inode_table_block = BGD[group].inode_table;
	index -= group * ext2->inodes_per_group;
	uint32_t block_offset = ((index - 1) * ext2->inode_size) / ext2->block_size;
	uint32_t offset_in_block = (index - 1) - block_offset * (ext2->block_size / ext2->inode_size);

	ext2_inodetable_t* inodet = kmalloc(ext2->block_size, GFP_KERNEL);

	read_block(ext2, inode_table_block + block_offset, (uint8_t*) inodet);
	memcpy((uint8_t*)((uint32_t) inodet + offset_in_block * ext2->inode_size), inode, ext2->inode_size);
	write_block(ext2, inode_table_block + block_offset, (uint8_t*) inodet);


	kfree(inodet);
	return E_OK;
}

static uint32_t allocate_block(ext2_fs_t * ext2) {
	uint32_t block_no = 0;
	uint32_t block_offset = 0;
	uint32_t group = 0;
	uint8_t* bg_buffer = kmalloc(ext2->block_size, GFP_KERNEL);

	for (uint32_t i = 0; i < BGDS; ++i) {
		if (BGD[i].free_blocks_count > 0) {
			read_block(ext2, BGD[i].block_bitmap, (uint8_t*)bg_buffer);
			while (BLOCKBIT(block_offset)) {
				++block_offset;
			}
			block_no = block_offset + SB->blocks_per_group * i;
			group = i;
			break;
		}
	}

	if (!block_no) {
		kprintf(ERROR, "No available blocks, disk is out of space!\n");
		kfree(bg_buffer);
		return 0;
	}

	kprintf(WARN "allocating block #%d (group %d)\n", block_no, group);

	BLOCKBYTE(block_offset) |= SETBIT(block_offset);
	write_block(ext2, BGD[group].block_bitmap, (uint8_t*) bg_buffer);

	BGD[group].free_blocks_count--;
	for (int i = 0; i < ext2->bgd_block_span; ++i)
		write_block(ext2, ext2->bgd_offset + i, (uint8_t*)((uint32_t)BGD + ext2->block_size * i));
	

	SB->free_blocks_count--;
	rewrite_superblock(ext2);

	memset(bg_buffer, 0x00, ext2->block_size);
	write_block(ext2, block_no, bg_buffer);

	kfree(bg_buffer);
	return block_no;
}



static int allocate_inode_block(ext2_fs_t * ext2, ext2_inodetable_t * inode, uint32_t inode_no, uint32_t block) {
	kprintf(INFO, "ext2: allocating block #%d for inode #%d\n", block, inode_no);
	uint32_t block_no = allocate_block(ext2);

	if (!block_no) {
        kprintf(ERROR "ext2: no space left on device!\n");

        errno = ENOSPC;
        return E_ERR;
    }

	set_block_number(ext2, inode, inode_no, block, block_no);

	uint32_t t = (block + 1) * (ext2->block_size / 512);
	if (inode->blocks < t) {
		kprintf(INFO "ext2: setting inode->blocks to %d = (%d fs blocks)\n", t, t / (ext2->block_size / 512));
		inode->blocks = t;
	}

	write_inode(ext2, inode, inode_no);
	return E_OK;
}



static uint32_t inode_read_block(ext2_fs_t* ext2, ext2_inodetable_t* inode, uint32_t block, uint8_t* buf) {

	if (block >= inode->blocks / (ext2->block_size / 512)) {
		memset(buf, 0x00, ext2->block_size);
		kprintf(WARN "ext2: tried to read an invalid block. Asked for %d (0-indexed), but inode only has %d!\n", block, inode->blocks / (ext2->block_size / 512));
		return 0;
	}

	uint32_t real_block = get_block_number(ext2, inode, block);
	read_block(ext2, real_block, buf);

	return real_block;
}



static uint32_t inode_write_block(ext2_fs_t* ext2, ext2_inodetable_t* inode, uint32_t inode_no, uint32_t block, uint8_t* buf) {
	if (block >= inode->blocks / (ext2->block_size / 512)) {
		kprintf(WARN "ext2: attempting to write beyond the existing allocated blocks for ext2 inode.");
		kprintf(WARN "ext2: inode %d, Block %d", inode_no, block);
	}

	kprintf(WARN "ext2: clearing and allocating up to required blocks (block=%d, %d)\n", block, inode->blocks);

	char * empty = NULL;
	while (block >= inode->blocks / (ext2->block_size / 512)) {
		allocate_inode_block(ext2, inode, inode_no, inode->blocks / (ext2->block_size / 512));
		refresh_inode(ext2, inode, inode_no);
	}

	if (empty) 
        kfree(empty);

	kprintf(WARN "ext2: ... done\n");

	uint32_t real_block = get_block_number(ext2, inode, block);
    kprintf(WARN "ext2: writing virtual block %d for inode %d maps to real block %d\n", block, inode_no, real_block);

	write_block(ext2, real_block, buf);
	return real_block;
}


static int create_entry(inode_t* parent, char* name, uint32_t inode) {
	ext2_fs_t* ext2 = (ext2_fs_t *) parent->userdata;

	ext2_inodetable_t * pinode = read_inode(ext2, parent->ino);
	if (((pinode->mode & EXT2_S_IFDIR) == 0) || (name == NULL)) {
		kprintf(WARN "ext2: attempted to allocate an inode in a parent that was not a directory.\n");
		return E_ERR;
	}

	kprintf(WARN "ext2: creating a directory entry for %s pointing to inode %d.\n", name, inode);
	kprintf(WARN "ext2: we need to append %d bytes to the directory.\n", sizeof(ext2_dir_t) + strlen(name));

	uint32_t rec_len = sizeof(ext2_dir_t) + strlen(name);
	rec_len += (rec_len % 4) ? (4 - (rec_len % 4)) : 0;

	kprintf(WARN "ext2: Our directory entry looks like ext2:\n");
	kprintf(WARN "ext2:   inode     = %d\n", inode);
	kprintf(WARN "ext2:   rec_len   = %d\n", rec_len);
	kprintf(WARN "ext2:   name_len  = %d\n", strlen(name));
	kprintf(WARN "ext2:   file_type = %d\n", 0);
	kprintf(WARN "ext2:   name      = %s\n", name);

	kprintf(WARN "ext2: The inode size is marked as: %d\n", pinode->size);
	kprintf(WARN "ext2: Block size is %d\n", ext2->block_size);

	uint8_t* block = kmalloc(ext2->block_size, GFP_KERNEL);
	uint8_t block_nr = 0;
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;
	int modify_or_replace = 0;
	ext2_dir_t* previous;

	inode_read_block(ext2, pinode, block_nr, block);
	while (total_offset < pinode->size) {
		if (dir_offset >= ext2->block_size) {
			block_nr++;
			dir_offset -= ext2->block_size;
			inode_read_block(ext2, pinode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t) block + dir_offset);

		uint32_t sreclen = d_ent->name_len + sizeof(ext2_dir_t);
		sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;

		{
			char f[d_ent->name_len + 1];
			memcpy(f, d_ent->name, d_ent->name_len);
			f[d_ent->name_len] = 0;

			kprintf(WARN "ext2:  * file: %s\n", f);
		}
		
		kprintf(WARN "ext2:    rec_len: %d\n", d_ent->rec_len);
		kprintf(WARN "ext2:    type: %d\n", d_ent->file_type);
		kprintf(WARN "ext2:    namel: %d\n", d_ent->name_len);
		kprintf(WARN "ext2:    inode: %d\n", d_ent->inode);

		if (d_ent->rec_len != sreclen && total_offset + d_ent->rec_len == pinode->size) {
			kprintf(WARN "ext2:   - should be %d, but instead points to end of block\n", sreclen);
			kprintf(WARN "ext2:   - we've hit the end, should change ext2 pointer\n");

			dir_offset += sreclen;
			total_offset += sreclen;

			modify_or_replace = 1;
			previous = d_ent;

			break;
		}

		if (d_ent->inode == 0)
			modify_or_replace = 2;
		

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}

	if (!modify_or_replace)
		kprintf(WARN "ext2: that's odd, ext2 shouldn't have happened, we made it all the way here without hitting our two end conditions?\n");
	

	if (modify_or_replace == 1) {
		kprintf(WARN "ext2: the last node in the list is a real node, we need to modify it.\n");

		if (dir_offset + rec_len >= ext2->block_size) {
			kprintf(WARN "ext2: need to allocate more space, bail!\n");
			kfree(block);

			kprintf(ERROR "ext2: no space left on device!\n");
            errno = ENOSPC;
            return E_ERR;
		} else {
			uint32_t sreclen = previous->name_len + sizeof(ext2_dir_t);
			sreclen += (sreclen % 4) ? (4 - (sreclen % 4)) : 0;
			previous->rec_len = sreclen;
			kprintf(WARN "ext2: set previous node rec_len to %d\n", sreclen);
		}

	} else if (modify_or_replace == 2)
		kprintf(WARN "ext2: the last node in the list is a fake node, we'll replace it.\n");
	

	kprintf(WARN "ext2:  total_offset = 0x%x\n", total_offset);
	kprintf(WARN "ext2:    dir_offset = 0x%x\n", dir_offset);
	ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

	d_ent->inode     = inode;
	d_ent->rec_len   = ext2->block_size - dir_offset;
	d_ent->name_len  = strlen(name);
	d_ent->file_type = 0;
	memcpy(d_ent->name, name, strlen(name));

	inode_write_block(ext2, pinode, parent->ino, block_nr, block);

	kfree(block);
	kfree(pinode);


	kprintf(ERROR "ext2: no space left on device!\n");
    errno = ENOSPC;
    return E_ERR;
}

static uint32_t allocate_inode(ext2_fs_t* ext2) {
	uint32_t node_no = 0;
	uint32_t node_offset = 0;
	uint32_t group = 0;
	uint8_t* bg_buffer = kmalloc(ext2->block_size, GFP_KERNEL);

	for (uint32_t i = 0; i < BGDS; ++i) {
		if (BGD[i].free_inodes_count > 0) {
			kprintf(LOG "ext2: group %d has %d free inodes.\n", i, BGD[i].free_inodes_count);

			read_block(ext2, BGD[i].inode_bitmap, (uint8_t*)bg_buffer);
			while (BLOCKBIT(node_offset))
				node_offset++;
			
			node_no = node_offset + i * ext2->inodes_per_group + 1;
			group = i;
			break;
		}
	}


	if (!node_no) {
		kprintf(ERROR "ext2: Ran out of inodes!\n");
		return 0;
	}

	BLOCKBYTE(node_offset) |= SETBIT(node_offset);

	write_block(ext2, BGD[group].inode_bitmap, (uint8_t*)bg_buffer);
	kfree(bg_buffer);

	BGD[group].free_inodes_count--;
	for (int i = 0; i < ext2->bgd_block_span; ++i)
		write_block(ext2, ext2->bgd_offset + i, (uint8_t*)((uint32_t)BGD + ext2->block_size * i));
	

	SB->free_inodes_count--;
	rewrite_superblock(ext2);

	return node_no;
}


static void mkdir_ext2(inode_t* parent, char* name, uint16_t permission) {
	if (!name)
        return;

	ext2_fs_t* ext2 = parent->userdata;

	inode_t* check = finddir_ext2(parent, name);
	if (unlikely(check)) {
		kprintf(WARN "ext2: A file by ext2 name already exists: %s\n", name);		
		return;
	}

	uint32_t inode_no = allocate_inode(ext2);
	ext2_inodetable_t* inode = read_inode(ext2, inode_no);

	inode->atime = timer_gettimestamp();
	inode->ctime = inode->atime;
	inode->mtime = inode->atime;
	inode->dtime = 0;

	memset(inode->block, 0x00, sizeof(inode->block));
	inode->blocks = 0;
	inode->size = 0;

	inode->uid = current_task->uid;
	inode->gid = current_task->gid;

	inode->faddr = 0;
	inode->links_count = 2;
	inode->flags = 0;
	inode->osd1 = 0;
	inode->generation = 0;
	inode->file_acl = 0;
	inode->dir_acl = 0;

	inode->mode = EXT2_S_IFDIR;
	inode->mode |= 0xFFF & permission;

	memset(inode->osd2, 0x00, sizeof(inode->osd2));

	write_inode(ext2, inode, inode_no);
	create_entry(parent, name, inode_no);

	inode->size = ext2->block_size;
	write_inode(ext2, inode, inode_no);

	uint8_t* tmp = kmalloc(ext2->block_size, GFP_KERNEL);
	ext2_dir_t* t = kcalloc(12, 1, GFP_KERNEL);
	t->inode = inode_no;
	t->rec_len = 12;
	t->name_len = 1;
	t->name[0] = '.';
	memcpy(&tmp[0], t, 12);

	t->inode = parent->inode;
	t->name_len = 2;
	t->name[1] = '.';
	t->rec_len = ext2->block_size - 12;
	memcpy(&tmp[12], t, 12);

	kfree(t);
	inode_write_block(ext2, inode, inode_no, 0, tmp);

	kfree(inode);
	kfree(tmp);

	ext2_inodetable_t * pinode = read_inode(ext2, parent->ino);
	pinode->links_count++;
	write_inode(ext2, pinode, parent->inode);
	kfree(pinode);

	uint32_t group = inode_no / ext2->inodes_per_group;
	BGD[group].used_dirs_count++;

	for (int i = 0; i < ext2->bgd_block_span; ++i)
		write_block(ext2, ext2->bgd_offset + i, (uint8_t*)((uint32_t)BGD + ext2->block_size * i));
	
}

static void create_ext2(inode_t* parent, char* name, uint16_t permission) {
	if (!name) 
		return;

	ext2_fs_t* ext2 = (ext2_fs_t*) parent->userdata;

	inode_t* check = finddir_ext2(parent, name);
	if (unlikely(check)) {
		kprintf(WARN "ext2: A file by ext2 name already exists: %s\n", name);
		return;
	}


	uint32_t inode_no = allocate_inode(ext2);
	ext2_inodetable_t * inode = read_inode(ext2,inode_no);

	inode->atime = timer_gettimestamp();
	inode->ctime = inode->atime;
	inode->mtime = inode->atime;
	inode->dtime = 0;

	memset(inode->block, 0x00, sizeof(inode->block));
	inode->blocks = 0;
	inode->size = 0;


	inode->uid = current_task->uid;
	inode->gid = current_task->gid;


	inode->faddr = 0;
	inode->links_count = 1;
	inode->flags = 0;
	inode->osd1 = 0;
	inode->generation = 0;
	inode->file_acl = 0;
	inode->dir_acl = 0;

	inode->mode = EXT2_S_IFREG;
	inode->mode |= 0xFFF & permission;

	memset(inode->osd2, 0x00, sizeof(inode->osd2));

	write_inode(ext2, inode, inode_no);
	create_entry(parent, name, inode_no);

	kfree(inode);
}

static int chmod_ext2(inode_t * node, int mode) {
	ext2_fs_t* ext2 = (ext2_fs_t*) node->userdata;

	ext2_inodetable_t* inode = read_inode(ext2, node->ino);
	inode->mode = (inode->mode & 0xFFFFF000) | mode;

	write_inode(ext2, inode, node->inode);
	return 0;
}

/**
 * direntry_ext2
 */
static ext2_dir_t * direntry_ext2(ext2_fs_t * ext2, ext2_inodetable_t * inode, uint32_t no, uint32_t index) {
	uint8_t*block = malloc(ext2->block_size);
	uint8_t block_nr = 0;
	inode_read_block(ext2, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;
	uint32_t dir_index = 0;

	while (total_offset < inode->size && dir_index <= index) {
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode != 0 && dir_index == index) {
			ext2_dir_t *out = malloc(d_ent->rec_len);
			memcpy(out, d_ent, d_ent->rec_len);
			free(block);
			return out;
		}

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;

		if (d_ent->inode) {
			dir_index++;
		}

		if (dir_offset >= ext2->block_size) {
			block_nr++;
			dir_offset -= ext2->block_size;
			inode_read_block(ext2, inode, block_nr, block);
		}
	}

	free(block);
	return NULL;
}

/**
 * finddir_ext2
 */
static inode_t * finddir_ext2(inode_t *node, char *name) {

	ext2_fs_t * ext2 = (ext2_fs_t *)node->device;

	ext2_inodetable_t *inode = read_inode(ext2,node->inode);
	assert(inode->mode & EXT2_S_IFDIR);
	uint8_t* block = malloc(ext2->block_size);
	ext2_dir_t *direntry = NULL;
	uint8_t block_nr = 0;
	inode_read_block(ext2, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;

	while (total_offset < inode->size) {
		if (dir_offset >= ext2->block_size) {
			block_nr++;
			dir_offset -= ext2->block_size;
			inode_read_block(ext2, inode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode == 0 || strlen(name) != d_ent->name_len) {
			dir_offset += d_ent->rec_len;
			total_offset += d_ent->rec_len;

			continue;
		}

		char *dname = malloc(sizeof(char) * (d_ent->name_len + 1));
		memcpy(dname, &(d_ent->name), d_ent->name_len);
		dname[d_ent->name_len] = '\0';
		if (!strcmp(dname, name)) {
			free(dname);
			direntry = malloc(d_ent->rec_len);
			memcpy(direntry, d_ent, d_ent->rec_len);
			break;
		}
		free(dname);

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}
	free(inode);
	if (!direntry) {
		free(block);
		return NULL;
	}
	inode_t *outnode = malloc(sizeof(inode_t));
	memset(outnode, 0, sizeof(inode_t));

	inode = read_inode(ext2, direntry->inode);

	if (!node_from_file(ext2, inode, direntry, outnode)) {
		debug_print(CRITICAL, "Oh dear. Couldn't allocate the outnode?");
	}

	free(direntry);
	free(inode);
	free(block);
	return outnode;
}

static void unlink_ext2(inode_t * node, char * name) {
	/* XXX ext2 is a very bad implementation */
	ext2_fs_t * ext2 = (ext2_fs_t *)node->device;

	ext2_inodetable_t *inode = read_inode(ext2,node->inode);
	assert(inode->mode & EXT2_S_IFDIR);
	uint8_t* block = malloc(ext2->block_size);
	ext2_dir_t *direntry = NULL;
	uint8_t block_nr = 0;
	inode_read_block(ext2, inode, block_nr, block);
	uint32_t dir_offset = 0;
	uint32_t total_offset = 0;

	while (total_offset < inode->size) {
		if (dir_offset >= ext2->block_size) {
			block_nr++;
			dir_offset -= ext2->block_size;
			inode_read_block(ext2, inode, block_nr, block);
		}
		ext2_dir_t *d_ent = (ext2_dir_t *)((uintptr_t)block + dir_offset);

		if (d_ent->inode == 0 || strlen(name) != d_ent->name_len) {
			dir_offset += d_ent->rec_len;
			total_offset += d_ent->rec_len;

			continue;
		}

		char *dname = malloc(sizeof(char) * (d_ent->name_len + 1));
		memcpy(dname, &(d_ent->name), d_ent->name_len);
		dname[d_ent->name_len] = '\0';
		if (!strcmp(dname, name)) {
			free(dname);
			direntry = d_ent;
			break;
		}
		free(dname);

		dir_offset += d_ent->rec_len;
		total_offset += d_ent->rec_len;
	}
	free(inode);
	if (!direntry) {
		free(block);
		return;
	}

	direntry->inode = 0;

	inode_write_block(ext2, inode, node->inode, block_nr, block);
	free(block);

	ext2_sync(ext2);
}


static void refresh_inode(ext2_fs_t * ext2, ext2_inodetable_t * inodet,  uint32_t inode) {
	uint32_t group = inode / ext2->inodes_per_group;
	if (group > BGDS)
		return;
	

	uint32_t inode_table_block = BGD[group].inode_table;
	inode -= group * ext2->inodes_per_group;
	uint32_t block_offset = ((inode - 1) * ext2->inode_size) / ext2->block_size;
	uint32_t offset_in_block = (inode - 1) - block_offset * (ext2->block_size / ext2->inode_size);

	uint8_t* buf = kmalloc(ext2->block_size, GFP_KERNEL);
	read_block(ext2, inode_table_block + block_offset, buf);

	ext2_inodetable_t *inodes = (ext2_inodetable_t *)buf;
	memcpy(inodet, (uint8_t*)((uint32_t)inodes + offset_in_block * ext2->inode_size), ext2->inode_size);
	kfree(buf);
}


static ext2_inodetable_t* read_inode(ext2_fs_t* ext2, uint32_t inode) {
	ext2_inodetable_t *inodet = kmalloc(ext2->inode_size, GFP_KERNEL);
	refresh_inode(ext2, inodet, inode);
	return inodet;
}

static uint32_t read_ext2(inode_t *node, uint32_t offset, uint32_t size, uint8_t*buffer) {
	ext2_fs_t * ext2 = (ext2_fs_t *)node->device;
	ext2_inodetable_t * inode = read_inode(ext2, node->inode);
	uint32_t end;
	if (inode->size == 0) return 0;
	if (offset + size > inode->size) {
		end = inode->size;
	} else {
		end = offset + size;
	}
	uint32_t start_block  = offset / ext2->block_size;
	uint32_t end_block    = end / ext2->block_size;
	uint32_t end_size     = end - end_block * ext2->block_size;
	uint32_t size_to_read = end - offset;

	uint8_t* buf = malloc(ext2->block_size);
	if (start_block == end_block) {
		inode_read_block(ext2, inode, start_block, buf);
		memcpy(buffer, (uint8_t*)(((uint32_t)buf) + (offset % ext2->block_size)), size_to_read);
	} else {
		uint32_t block_offset;
		uint32_t blocks_read = 0;
		for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
			if (block_offset == start_block) {
				inode_read_block(ext2, inode, block_offset, buf);
				memcpy(buffer, (uint8_t*)(((uint32_t)buf) + (offset % ext2->block_size)), ext2->block_size - (offset % ext2->block_size));
			} else {
				inode_read_block(ext2, inode, block_offset, buf);
				memcpy(buffer + ext2->block_size * blocks_read - (offset % ext2->block_size), buf, ext2->block_size);
			}
		}
		if (end_size) {
			inode_read_block(ext2, inode, end_block, buf);
			memcpy(buffer + ext2->block_size * blocks_read - (offset % ext2->block_size), buf, end_size);
		}
	}
	free(inode);
	free(buf);
	return size_to_read;
}

static uint32_t write_inode_buffer(ext2_fs_t * ext2, ext2_inodetable_t * inode, uint32_t inode_number, uint32_t offset, uint32_t size, uint8_t*buffer) {
	uint32_t end = offset + size;
	if (end > inode->size) {
		inode->size = end;
		write_inode(ext2, inode, inode_number);
	}

	uint32_t start_block  = offset / ext2->block_size;
	uint32_t end_block    = end / ext2->block_size;
	uint32_t end_size     = end - end_block * ext2->block_size;
	uint32_t size_to_read = end - offset;
	uint8_t* buf = malloc(ext2->block_size);
	if (start_block == end_block) {
		inode_read_block(ext2, inode, start_block, buf);
		memcpy((uint8_t*)(((uint32_t)buf) + (offset % ext2->block_size)), buffer, size_to_read);
		inode_write_block(ext2, inode, inode_number, start_block, buf);
	} else {
		uint32_t block_offset;
		uint32_t blocks_read = 0;
		for (block_offset = start_block; block_offset < end_block; block_offset++, blocks_read++) {
			if (block_offset == start_block) {
				int b = inode_read_block(ext2, inode, block_offset, buf);
				memcpy((uint8_t*)(((uint32_t)buf) + (offset % ext2->block_size)), buffer, ext2->block_size - (offset % ext2->block_size));
				inode_write_block(ext2, inode, inode_number, block_offset, buf);
				if (!b) {
					refresh_inode(ext2, inode, inode_number);
				}
			} else {
				int b = inode_read_block(ext2, inode, block_offset, buf);
				memcpy(buf, buffer + ext2->block_size * blocks_read - (offset % ext2->block_size), ext2->block_size);
				inode_write_block(ext2, inode, inode_number, block_offset, buf);
				if (!b) {
					refresh_inode(ext2, inode, inode_number);
				}
			}
		}
		if (end_size) {
			inode_read_block(ext2, inode, end_block, buf);
			memcpy(buf, buffer + ext2->block_size * blocks_read - (offset % ext2->block_size), end_size);
			inode_write_block(ext2, inode, inode_number, end_block, buf);
		}
	}
	free(buf);
	return size_to_read;
}

static uint32_t write_ext2(inode_t *node, uint32_t offset, uint32_t size, uint8_t*buffer) {
	ext2_fs_t * ext2 = (ext2_fs_t *)node->device;
	ext2_inodetable_t * inode = read_inode(ext2, node->inode);

	uint32_t rv = write_inode_buffer(ext2, inode, node->inode, offset, size, buffer);
	free(inode);
	return rv;
}

static void open_ext2(inode_t *node, uint32_t flags) {
	ext2_fs_t * ext2 = node->device;

	if (flags & O_TRUNC) {
		/* Uh, herp */
		ext2_inodetable_t * inode = read_inode(ext2,node->inode);
		inode->size = 0;
		write_inode(ext2, inode, node->inode);
	}
}

static void close_ext2(inode_t *node) {
	/* Nothing to do here */
}


/**
 * readdir_ext2
 */
static struct dirent * readdir_ext2(inode_t *node, uint32_t index) {

	ext2_fs_t * ext2 = (ext2_fs_t *)node->device;

	ext2_inodetable_t *inode = read_inode(ext2, node->inode);
	assert(inode->mode & EXT2_S_IFDIR);
	ext2_dir_t *direntry = direntry_ext2(ext2, inode, node->inode, index);
	if (!direntry) {
		free(inode);
		return NULL;
	}
	struct dirent *dirent = malloc(sizeof(struct dirent));
	memcpy(&dirent->name, &direntry->name, direntry->name_len);
	dirent->name[direntry->name_len] = '\0';
	dirent->ino = direntry->inode;
	free(direntry);
	free(inode);
	return dirent;
}




static uint32_t node_from_file(ext2_fs_t * ext2, ext2_inodetable_t *inode, ext2_dir_t *direntry,  inode_t *fnode) {
	if (!fnode) {
		/* You didn't give me a node to write into, go **** yourself */
		return 0;
	}
	/* Information from the direntry */
	fnode->device = (void *)ext2;
	fnode->inode = direntry->inode;
	memcpy(&fnode->name, &direntry->name, direntry->name_len);
	fnode->name[direntry->name_len] = '\0';
	/* Information from the inode */
	fnode->uid = inode->uid;
	fnode->gid = inode->gid;
	fnode->length = inode->size;
	fnode->mask = inode->mode & 0xFFF;
	fnode->nlink = inode->links_count;
	/* File Flags */
	fnode->flags = 0;
	if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
		fnode->flags   |= FS_FILE;
		fnode->read     = read_ext2;
		fnode->write    = write_ext2;
		fnode->create   = NULL;
		fnode->mkdir    = NULL;
		fnode->readdir  = NULL;
		fnode->finddir  = NULL;
		fnode->symlink  = NULL;
		fnode->readlink = NULL;
	}
	if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
		fnode->flags   |= FS_DIRECTORY;
		fnode->create   = create_ext2;
		fnode->mkdir    = mkdir_ext2;
		fnode->readdir  = readdir_ext2;
		fnode->finddir  = finddir_ext2;
		fnode->unlink   = unlink_ext2;
		fnode->write    = NULL;
		fnode->symlink  = symlink_ext2;
		fnode->readlink = NULL;
	}
	if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
		fnode->flags |= FS_BLOCKDEVICE;
	}
	if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
		fnode->flags |= FS_CHARDEVICE;
	}
	if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
		fnode->flags |= FS_PIPE;
	}
	if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
		fnode->flags   |= FS_SYMLINK;
		fnode->read     = NULL;
		fnode->write    = NULL;
		fnode->create   = NULL;
		fnode->mkdir    = NULL;
		fnode->readdir  = NULL;
		fnode->finddir  = NULL;
		fnode->readlink = readlink_ext2;
	}

	fnode->atime   = inode->atime;
	fnode->mtime   = inode->mtime;
	fnode->ctime   = inode->ctime;
	debug_print(INFO, "file a/m/c times are %d/%d/%d", fnode->atime, fnode->mtime, fnode->ctime);

	fnode->chmod   = chmod_ext2;
	fnode->open    = open_ext2;
	fnode->close   = close_ext2;
	fnode->ioctl   = NULL;
	return 1;
}

static uint32_t ext2_root(ext2_fs_t * ext2, ext2_inodetable_t *inode, inode_t *fnode) {
	if (!fnode) {
		return 0;
	}
	/* Information for root dir */
	fnode->device = (void *)ext2;
	fnode->inode = 2;
	fnode->name[0] = '/';
	fnode->name[1] = '\0';
	/* Information from the inode */
	fnode->uid = inode->uid;
	fnode->gid = inode->gid;
	fnode->length = inode->size;
	fnode->mask = inode->mode & 0xFFF;
	fnode->nlink = inode->links_count;
	/* File Flags */
	fnode->flags = 0;
	if ((inode->mode & EXT2_S_IFREG) == EXT2_S_IFREG) {
		debug_print(CRITICAL, "Root appears to be a regular file.");
		debug_print(CRITICAL, "ext2 is probably very, very wrong.");
		return 0;
	}
	if ((inode->mode & EXT2_S_IFDIR) == EXT2_S_IFDIR) {
	} else {
		debug_print(CRITICAL, "Root doesn't appear to be a directory.");
		debug_print(CRITICAL, "ext2 is probably very, very wrong.");

		kprintf(ERROR "ext2: Other useful information:");
		kprintf(ERROR "ext2: %d", inode->uid);
		kprintf(ERROR "ext2: %d", inode->gid);
		kprintf(ERROR "ext2: %d", inode->size);
		kprintf(ERROR "ext2: %d", inode->mode);
		kprintf(ERROR "ext2: %d", inode->links_count);

		return 0;
	}
	if ((inode->mode & EXT2_S_IFBLK) == EXT2_S_IFBLK) {
		fnode->flags |= FS_BLOCKDEVICE;
	}
	if ((inode->mode & EXT2_S_IFCHR) == EXT2_S_IFCHR) {
		fnode->flags |= FS_CHARDEVICE;
	}
	if ((inode->mode & EXT2_S_IFIFO) == EXT2_S_IFIFO) {
		fnode->flags |= FS_PIPE;
	}
	if ((inode->mode & EXT2_S_IFLNK) == EXT2_S_IFLNK) {
		fnode->flags |= FS_SYMLINK;
	}

	fnode->atime   = inode->atime;
	fnode->mtime   = inode->mtime;
	fnode->ctime   = inode->ctime;

	fnode->flags |= FS_DIRECTORY;
	fnode->read    = NULL;
	fnode->write   = NULL;
	fnode->chmod   = chmod_ext2;
	fnode->open    = open_ext2;
	fnode->close   = close_ext2;
	fnode->readdir = readdir_ext2;
	fnode->finddir = finddir_ext2;
	fnode->ioctl   = NULL;
	fnode->create  = create_ext2;
	fnode->mkdir   = mkdir_ext2;
	fnode->unlink  = unlink_ext2;
	return 1;
}

static inode_t * mount_ext2(inode_t * block_device, int flags) {

	kprintf(LOG "ext2: Mounting ext2 file system...");
	ext2_fs_t * ext2 = malloc(sizeof(ext2_fs_t));

	memset(ext2, 0x00, sizeof(ext2_fs_t));

	ext2->flags = flags;

	ext2->block_device = block_device;
	ext2->block_size = 1024;
	vfs_lock(ext2->block_device);

	SB = malloc(ext2->block_size);

	debug_print(INFO, "Reading superblock...");
	read_block(ext2, 1, (uint8_t*)SB);
	if (SB->magic != EXT2_SUPER_MAGIC) {
		kprintf(ERROR "ext2: ... not an EXT2 filesystem? (magic didn't match, got 0x%x)", SB->magic);
		return NULL;
	}
	ext2->inode_size = SB->inode_size;
	if (SB->inode_size == 0) {
		ext2->inode_size = 128;
	}
	ext2->block_size = 1024 << SB->log_block_size;
	ext2->cache_entries = 10240;
	if (ext2->block_size > 2048) {
		ext2->cache_entries /= 4;
	}
	debug_print(INFO, "bs=%d, cache entries=%d", ext2->block_size, ext2->cache_entries);
	ext2->pointers_per_block = ext2->block_size / 4;
	debug_print(INFO, "Log block size = %d -> %d", SB->log_block_size, ext2->block_size);
	BGDS = SB->blocks_count / SB->blocks_per_group;
	if (SB->blocks_per_group * BGDS < SB->blocks_count) {
		BGDS += 1;
	}
	ext2->inodes_per_group = SB->inodes_count / BGDS;

	DC = NULL;

	// load the block group descriptors
	ext2->bgd_block_span = sizeof(ext2_bgdescriptor_t) * BGDS / ext2->block_size + 1;
	BGD = kmalloc(ext2->block_size * ext2->bgd_block_span, GFP_KERNEL);

	debug_print(INFO, "bgd_block_span = %d", ext2->bgd_block_span);

	ext2->bgd_offset = 2;

	if (ext2->block_size > 1024) {
		ext2->bgd_offset = 1;
	}

	for (int i = 0; i < ext2->bgd_block_span; ++i) {
		read_block(ext2, ext2->bgd_offset + i, (uint8_t*)((uint32_t)BGD + ext2->block_size * i));
	}

#ifdef DEBUG_BLOCK_DESCRIPTORS
	char * bg_buffer = malloc(ext2->block_size * sizeof(char));
	for (uint32_t i = 0; i < BGDS; ++i) {
		debug_print(INFO, "Block Group Descriptor #%d @ %d", i, ext2->bgd_offset + i * SB->blocks_per_group);
		debug_print(INFO, "\tBlock Bitmap @ %d", BGD[i].block_bitmap); {
			debug_print(INFO, "\t\tExamining block bitmap at %d", BGD[i].block_bitmap);
			read_block(ext2, BGD[i].block_bitmap, (uint8_t*)bg_buffer);
			uint32_t j = 0;
			while (BLOCKBIT(j)) {
				++j;
			}
			debug_print(INFO, "\t\tFirst free block in group is %d", j + BGD[i].block_bitmap - 2);
		}
		debug_print(INFO, "\tInode Bitmap @ %d", BGD[i].inode_bitmap); {
			debug_print(INFO, "\t\tExamining inode bitmap at %d", BGD[i].inode_bitmap);
			read_block(ext2, BGD[i].inode_bitmap, (uint8_t*)bg_buffer);
			uint32_t j = 0;
			while (BLOCKBIT(j)) {
				++j;
			}
			debug_print(INFO, "\t\tFirst free inode in group is %d", j + ext2->inodes_per_group * i + 1);
		}
		debug_print(INFO, "\tInode Table  @ %d", BGD[i].inode_table);
		debug_print(INFO, "\tFree Blocks =  %d", BGD[i].free_blocks_count);
		debug_print(INFO, "\tFree Inodes =  %d", BGD[i].free_inodes_count);
	}
	free(bg_buffer);
#endif

	ext2_inodetable_t *root_inode = read_inode(ext2, 2);
	RN = (inode_t *)malloc(sizeof(inode_t));
	if (!ext2_root(ext2, root_inode, RN)) {
		return NULL;
	}
	kprintf(LOG "ext2: Mounted EXT2 disk, root VFS node is at 0x%x", RN);
	return RN;
}

