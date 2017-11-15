#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

#include "ext2.h"

MODULE_NAME("fs/ext2");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


void ext2_read_inode(ext2_t* priv, ext2_inode_t* inodebuf, uint32_t inode) {
    block_group_desc_t* bgd;
    ext2_read_block(priv, priv->cache, priv->first_bgd);

    bgd = (block_group_desc_t*) priv->cache;
    for(int i = 0; i < ((inode - 1) / priv->sb.inodes_in_blockgroup); i++)
        bgd++;


    uint32_t block = (((inode - 1) % priv->sb.inodes_in_blockgroup) * sizeof(ext2_inode_t)) / priv->blocksize;
    ext2_read_block(priv, priv->cache, bgd->block_of_inode_table + block);


    ext2_inode_t* in = (ext2_inode_t*) priv->cache;
    for(int i = 0; i < (((inode - 1) % priv->sb.inodes_in_blockgroup) % priv->inodes_per_block); i++)
        in++;

    memcpy(inodebuf, in, sizeof(inode_t));
}

void ext2_write_inode(ext2_t* priv, ext2_inode_t* inodebuf, uint32_t inode) {
    block_group_desc_t* bgd;
    ext2_read_block(priv, priv->cache, priv->first_bgd);

    bgd = (block_group_desc_t*) priv->cache;
    for(int i = 0; i < ((inode - 1) / priv->sb.inodes_in_blockgroup); i++)
        bgd++;

    uint32_t block = (((inode - 1) % priv->sb.inodes_in_blockgroup) * sizeof(ext2_inode_t)) / priv->blocksize;
    ext2_read_block(priv, priv->cache, bgd->block_of_inode_table + block);

    ext2_inode_t* in = (ext2_inode_t*) priv->cache;
    for(int i = 0; i < (((inode - 1) % priv->sb.inodes_in_blockgroup) % priv->inodes_per_block); i++)
        in++;

    memcpy(in, inodebuf, sizeof(inode_t));
    ext2_write_block(priv, priv->cache, bgd->block_of_inode_table + block);
}

void ext2_get_inode_block(ext2_t* priv, uint32_t inode, uint32_t* b, uint32_t* ioff) {
    block_group_desc_t* bgd;
    ext2_read_block(priv, priv->cache, priv->first_bgd);

    bgd = (block_group_desc_t*) priv->cache;
    for(int i = 0; i < ((inode - 1) / priv->sb.inodes_in_blockgroup); i++)
        bgd++;

    *b = bgd->block_of_inode_table + (((inode - 1) % priv->sb.inodes_in_blockgroup) * sizeof(ext2_inode_t)) / priv->blocksize;
    *ioff = (((inode - 1) % priv->sb.inodes_in_blockgroup) % priv->inodes_per_block);
}

void ext2_get_blockchain(ext2_t* priv, ext2_inode_t* in, uint32_t** pchain) {
    if(!in->size)
        return;
    
    uint32_t* chain = (uint32_t*) kmalloc(((in->size / priv->blocksize) + 1) * sizeof(uint32_t), GFP_KERNEL);
    uint32_t p = 0;
    
    if(pchain)
        *pchain = chain;


    int i;
    for(i = 0; i < 12; i++) {
        if(!in->dbp[i])
            return;

        chain[p++] = in->dbp[i];
    }

    if(in->singly_block) {
        ext2_read_block(priv, priv->cache, in->singly_block);

        for(i = 0; i < (priv->blocksize / sizeof(uint32_t)); i++) {
            if(!((uint32_t*) priv->cache) [i])
                return;

            chain[p++] = ((uint32_t*) priv->cache) [i];
        }
    }

    if(in->doubly_block) {
        uint32_t cache[priv->blocksize];
        ext2_read_block(priv, cache, in->doubly_block);

        for(i = 0; i < (priv->blocksize / sizeof(uint32_t)); i++) {
            if(!cache[i])
                return;
                
            ext2_read_block(priv, priv->cache, cache[i]);

            int j;
            for(j = 0; j < (priv->blocksize / sizeof(uint32_t)); j++) {
                if(!((uint32_t*) priv->cache) [j])
                    return;

                chain[p++] = ((uint32_t*) priv->cache) [j];
            }
        }
    }

    if(in->triply_block) {
        uint32_t tcache[priv->blocksize];
        ext2_read_block(priv, tcache, in->triply_block);

        int k;
        for(k = 0; k < (priv->blocksize / sizeof(uint32_t)); k++) {
            if(!tcache[k])
                return;
                
            uint32_t cache[priv->blocksize];
            ext2_read_block(priv, cache, tcache[k]);

            for(i = 0; i < (priv->blocksize / sizeof(uint32_t)); i++) {
                if(!cache[i])
                    return;
                    
                ext2_read_block(priv, priv->cache, cache[i]);

                int j;
                for(j = 0; j < (priv->blocksize / sizeof(uint32_t)); j++) {
                    if(!((uint32_t*) priv->cache) [j])
                        return;

                    chain[p++] = ((uint32_t*) priv->cache) [j];
                }
            }
        }
    }
}


void ext2_mkchild(ext2_t* priv, inode_t** pchild, inode_t* parent, uint32_t ino, char* name) {
    ext2_priv_t* p = (ext2_priv_t*) kmalloc(sizeof(ext2_priv_t), GFP_KERNEL);
    ext2_read_inode(priv, &p->inode, ino);
    
    inode_t* child = (inode_t*) kmalloc(sizeof(inode_t), GFP_KERNEL);
    memset(child, 0, sizeof(inode_t));

    child->name = strdup(name);
    child->userdata = (void*) p;
    child->ino = ino;
    child->mode = p->inode.type;
    
    child->dev =
    child->rdev = 0;
    child->nlink = p->inode.hardlinks;
    
    child->uid = p->inode.uid;
    child->gid = p->inode.gid;
    child->size = (off64_t) p->inode.size;
    
    child->atime = p->inode.last_access;
    child->mtime = p->inode.last_modif;
    child->ctime = p->inode.create_time;

    child->parent = parent;
    child->link = NULL;
    child->childs = NULL;
    
    p->ext2 = priv;
    p->blockchain = NULL;



    if((p->inode.type & 0xF000) == INODE_TYPE_DIRECTORY) {
        child->open = ext2_open;
        child->finddir = ext2_finddir;
    } else {
        child->read = ext2_read;

        ext2_get_blockchain(priv, &p->inode, &p->blockchain);
    }


    if(pchild)
        *pchild = child;
}

static int ext2_mount(inode_t* dev, inode_t* dir) {
    
    superblock_t sb;
    vfs_read(dev, &sb, 1024, 1024);

    if(sb.ext2_sig != EXT2_SIGNATURE) {
        kprintf(ERROR "ext2: invalid signature!\n");

        errno = EINVAL;
        return E_ERR;
    }

    ext2_t* priv = (ext2_t*) kmalloc(sizeof(ext2_t), GFP_KERNEL);
    memcpy(&priv->sb, &sb, sizeof(superblock_t));


    priv->blocksize = 1024 << sb.blocksize_hint;
    priv->inodes_per_block = priv->blocksize / sizeof(ext2_inode_t);
    priv->sectors_per_block = priv->blocksize / 512;
    priv->number_of_bgs = sb.blocks / sb.blocks_in_blockgroup;
    priv->first_bgd = sb.superblock_id + (sizeof(superblock_t) / priv->blocksize);
    priv->dev = dev;
    priv->cache = (void*) kmalloc(priv->blocksize, GFP_KERNEL);

    if(priv->number_of_bgs == 0)
        priv->number_of_bgs++;

    if(priv->first_bgd == 0)
        priv->first_bgd++;


    dir->childs = NULL;
    dir->ino = 2;
    dir->open = ext2_open;
    dir->finddir = ext2_finddir;


    ext2_priv_t* p = (ext2_priv_t*) kmalloc(sizeof(ext2_priv_t), GFP_KERNEL);
    ext2_read_inode(priv, &p->inode, dir->ino);
    p->ext2 = priv;
    p->blockchain = NULL;

    dir->userdata = (void*) p;

    return E_OK;
}

int init(void) {
    if(vfs_fsys_register("ext2", ext2_mount) != E_OK)
        return E_ERR;

    return E_OK;
}



int dnit(void) {
    return E_OK;
}
