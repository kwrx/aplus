#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/timer.h>
#include <aplus/ipc.h>
#include <aplus/module.h>
#include <aplus/debug.h>

#if CONFIG_CACHE
static kcache_pool_t* pool_queue = NULL;


static void* __kcache_alloc_block(kcache_pool_t* pool, kcache_index_t index) {
    if(unlikely(!pool))
        return NULL;

    kcache_block_t* blk = (kcache_block_t*) kmalloc(sizeof(kcache_block_t), GFP_KERNEL);
    void* blkptr = (void*) kmalloc(pool->blksize, GFP_KERNEL);

    KASSERT(blk && blkptr);

    blk->index = index;
    blk->ptr = blkptr;
    blk->next = pool->blocks;

    pool->blocks = blk;
    pool->cachesize += 1;


    spinlock_init(&blk->lock);
    spinlock_lock(&blk->lock);
    return blkptr;
}


void kcache_free(int mode) {
    kcache_pool_t* p = NULL, 
                 *fb = NULL;

    switch(mode) {
        case KCACHE_FREE_ALL:
            for(; pool_queue; )
                kcache_unregister_pool(pool_queue);
            break;
        case KCACHE_FREE_BLOCKS:
            for(p = pool_queue; p; p = p->next)
                kcache_free_pool(p);
            break;
        case KCACHE_FREE_OLDEST: {
            ktime_t ll = ~0;
            for(p = pool_queue; p; p = p->next) {
                if(p->last_access < ll) {
                    ll = p->last_access;
                    fb = p;
                }
            }

            KASSERT(fb);
            kcache_free_pool(fb);
            } break;
    }
    
}


void kcache_free_pool(kcache_pool_t* pool) {
    kcache_block_t* blk;
    for(; pool->blocks; ) {
        blk = pool->blocks;
        pool->blocks = blk->next;

        spinlock_lock(&blk->lock);
        kfree(blk->ptr);
        spinlock_unlock(&blk->lock);
        kfree(blk);
    }

    pool->last_access = timer_getticks();
    pool->cachesize = 0;
}

void kcache_register_pool(kcache_pool_t* pool, size_t blksize) {
    memset(pool, 0, sizeof(kcache_pool_t));

    pool->blksize = blksize;
    pool->next = pool_queue;
    pool_queue = pool;

    pool->last_access = timer_getticks();
}

void kcache_unregister_pool(kcache_pool_t* pool) {
    if(pool == pool_queue)
        pool_queue = pool->next;
    else {
        kcache_pool_t* p;
        for(p = pool_queue; p; p = p->next) {
            if(p->next != pool)
                continue;
            
            p->next = pool->next;
            break;
        }
    }

    kcache_free_pool(pool);
}


void kcache_free_block(kcache_pool_t* pool, kcache_index_t index) {
    kcache_block_t* blk = pool->blocks;

    if(blk->index == index) {
        spinlock_lock(&blk->lock);
        pool->blocks = blk->next;
        pool->cachesize -= 1;
        pool->last_access = timer_getticks();

        kfree(blk->ptr);
        spinlock_unlock(&blk->lock);
        kfree(blk);

        return;
    }

    
    for(; blk->next; blk = blk->next)
        if(blk->next->index == index)
            break;

    if(unlikely(!blk->next))
        return;

    spinlock_lock(&blk->lock);
    pool->cachesize -= 1;
    pool->last_access = timer_getticks();

    register kcache_block_t* tmp = blk->next;
    blk->next = blk->next->next;

    kfree(tmp->ptr);
    spinlock_unlock(&blk->lock);

    kfree(tmp);
    return;
}


int kcache_obtain_block(kcache_pool_t* pool, kcache_index_t index, void** ptr) {
    kcache_block_t* blk;
    for(blk = pool->blocks; blk; blk = blk->next)
        if(unlikely(blk->index == index))
            break;

    pool->last_access = timer_getticks();

    if(unlikely(!blk)) {
        if(likely(ptr))
            *ptr = __kcache_alloc_block(pool, index);

        return -1;
    }

    spinlock_lock(&blk->lock);
    if(likely(ptr))
        *ptr = blk->ptr;

    return 0;
}

void kcache_release_block(kcache_pool_t* pool, kcache_index_t index) {
    kcache_block_t* blk;
    for(blk = pool->blocks; blk; blk = blk->next)
        if(unlikely(blk->index == index))
            break;

    if(unlikely(!blk))
        return;

    pool->last_access = timer_getticks();

    spinlock_unlock(&blk->lock);
}


EXPORT(kcache_free);
EXPORT(kcache_free_pool);
EXPORT(kcache_register_pool);
EXPORT(kcache_unregister_pool);
EXPORT(kcache_free_block);
EXPORT(kcache_obtain_block);
EXPORT(kcache_release_block);

#endif