#ifndef _EXT2_H
#define _EXT2_H

#include <aplus.h>
#include <aplus/base.h>
#include <libc.h>

#define EXT2_SIGNATURE                  0xEF53
#define EXT2_SECTOR_SIZE                512

#define INODE_TYPE_FIFO                 0x1000
#define INODE_TYPE_CHAR_DEV             0x2000
#define INODE_TYPE_DIRECTORY            0x4000
#define INODE_TYPE_BLOCK_DEV            0x6000
#define INODE_TYPE_FILE                 0x8000
#define INODE_TYPE_SYMLINK              0xA000
#define INODE_TYPE_SOCKET               0xC000


typedef struct {
    uint32_t inodes;
    uint32_t blocks;
    uint32_t reserved_for_root;
    uint32_t unallocatedblocks;
    uint32_t unallocatedinodes;
    uint32_t superblock_id;
    uint32_t blocksize_hint;
    uint32_t fragmentsize_hint;
    uint32_t blocks_in_blockgroup;
    uint32_t frags_in_blockgroup;
    uint32_t inodes_in_blockgroup;
    uint32_t last_mount;
    uint32_t last_write;
    uint16_t mounts_since_last_check;
    uint16_t max_mounts_since_last_check;
    uint16_t ext2_sig;
    uint16_t state;
    uint16_t op_on_err;
    uint16_t minor_version;
    uint32_t last_check;
    uint32_t max_time_in_checks;
    uint32_t os_id;
    uint32_t major_version;
    uint16_t uuid;
    uint16_t gid;
    uint8_t unused[940];
} __attribute__((packed)) superblock_t;


typedef struct {
    uint32_t block_of_block_usage_bitmap;
    uint32_t block_of_inode_usage_bitmap;
    uint32_t block_of_inode_table;
    uint16_t num_of_unalloc_block;
    uint16_t num_of_unalloc_inode;
    uint16_t num_of_dirs;
    uint8_t unused[14];
} __attribute__((packed)) block_group_desc_t;


typedef struct {
    uint16_t type;
    uint16_t uid;
    uint32_t size;
    uint32_t last_access;
    uint32_t create_time;
    uint32_t last_modif;
    uint32_t delete_time;
    uint16_t gid;
    uint16_t hardlinks;
    uint32_t disk_sectors;
    uint32_t flags;
    uint32_t ossv1;
    uint32_t dbp[12];
    uint32_t singly_block;
    uint32_t doubly_block;
    uint32_t triply_block;
    uint32_t gen_no;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t fragment_block;
    uint8_t ossv2[12];
} __attribute__((packed)) ext2_inode_t;


typedef struct __ext2_dir_entry {
    uint32_t inode;
    uint16_t size;
    uint8_t namelength;
    uint8_t reserved;
    char name[1];
} __attribute__((packed)) ext2_dir_t;


typedef struct __ext2_priv_data {
    superblock_t sb;
    uint32_t first_bgd;
    uint32_t number_of_bgs;
    uint32_t blocksize;
    uint32_t sectors_per_block;
    uint32_t inodes_per_block;
    ext2_inode_t root_inode;

    void* cache;
    size_t cachesize;
    inode_t* dev;
} __attribute__((packed)) ext2_t;

typedef struct {
    ext2_t* priv;
    uint32_t* blockchain;
} ext2_priv_t;




#define ext2_read_block(priv, buf, block)                                           \
    {                                                                               \
        priv->dev->position = (block) * priv->sectors_per_block * EXT2_SECTOR_SIZE; \
        vfs_read(priv->dev, buf, priv->sectors_per_block * EXT2_SECTOR_SIZE);       \
    }

#define ext2_write_block(priv, buf, block)                                          \
    {                                                                               \
        priv->dev->position = (block) * priv->sectors_per_block * EXT2_SECTOR_SIZE; \
        vfs_write(priv->dev, buf, priv->sectors_per_block * EXT2_SECTOR_SIZE);      \
    }



int ext2_open(struct inode* inode);
int ext2_read(inode_t* ino, void* buf, size_t size);
inode_t* ext2_finddir(struct inode* inode, char* filename);

void ext2_get_blockchain(ext2_t* priv, ext2_inode_t* in, uint32_t** pchain);
void ext2_get_inode_block(ext2_t* priv, uint32_t inode, uint32_t* b, uint32_t* ioff);
void ext2_write_inode(ext2_t* priv, ext2_inode_t* inodebuf, uint32_t inode);
void ext2_read_inode(ext2_t* priv, ext2_inode_t* inodebuf, uint32_t inode);
void ext2_mkchild(ext2_t* priv, inode_t** child, inode_t* parent, uint32_t ino, char* name);

#endif