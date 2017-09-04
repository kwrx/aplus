#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/task.h>
#include <aplus/timer.h>
#include <libc.h>

#include "ext2.h"



int ext2_read(inode_t* ino, void* buf, size_t size) {
    if(unlikely(!ino))
		return 0;
		
	if(unlikely(!buf))
		return 0;
		
	if(unlikely((off64_t) size > ino->size))
		size = ino->size;
		
	if(unlikely(ino->position > ino->size))
		ino->position = ino->size;
		
	if(unlikely(ino->position + (off64_t) size > ino->size))
		size = (off_t) (ino->size - ino->position);
		
	if(unlikely(!size))
		return 0;

    ext2_priv_t* priv = (ext2_priv_t*) ino->userdata;
	
    ext2_inode_t in;
    ext2_read_inode(priv->priv, &in, (uint32_t) ino->ino);

    if(!priv->blockchain) {
        errno = EIO;
        return 0;
    }
    
    uint32_t sb = ino->position / priv->priv->blocksize;
    uint32_t eb = (ino->position + size - 1) / priv->priv->blocksize;
    off64_t off = 0;

    if(ino->position % priv->priv->blocksize) {
        long p;
        p = priv->priv->blocksize - (ino->position % priv->priv->blocksize);
        p = p > size ? size : p;

        ext2_read_block(priv->priv, priv->priv->cache, priv->blockchain[sb]);
        memcpy(buf, (void*) ((uintptr_t) priv->priv->cache + ((uintptr_t) ino->position % priv->priv->blocksize)), p);
    
        off += p;
        sb++;
    }


    if(((ino->position + size) % priv->priv->blocksize) && (sb <= eb)) {
        long p = (ino->position + size) % priv->priv->blocksize;

        ext2_read_block(priv->priv, priv->priv->cache, priv->blockchain[eb]);
        memcpy((void*) ((uintptr_t) buf + size - p), priv->priv->cache, p);
        eb--;
    }

    long i = eb - sb + 1;
    if(likely(i > 0)) {
        for(; i--; off += priv->priv->blocksize)
            ext2_read_block(priv->priv, (void*) ((uintptr_t) buf + (uintptr_t) off), priv->blockchain[sb++]);
    }


    ino->position += size;
    return size;
}