#include <aplus.h>
#include <aplus/base.h>
#include <aplus/mm.h>
#include <aplus/timer.h>
#include <aplus/ipc.h>
#include <aplus/module.h>
#include <aplus/debug.h>
#include <libc.h>


#if CONFIG_CACHE
#define CACHESIZE                (kvm_state()->total >> 1)

static kcache_pool_t* pool_queue = NULL;
static uint64_t cachesize = 0;


static void* __kcache_alloc_block(kcache_pool_t* pool, kcache_index_t index, size_t size) {
    if(unlikely(!pool)) {
        errno = EINVAL;
        return NULL;
    }

    if(size > CACHESIZE) {
        errno = ENOMEM;
        return NULL;
    }

    while(cachesize + size > CACHESIZE) {
        if(kcache_free(KCACHE_FREE_OLDEST) > 0)
            continue;
             
        errno = ENOMEM;
        return NULL;
    }

    kcache_block_t* blk = (kcache_block_t*) kmalloc(sizeof(kcache_block_t), GFP_KERNEL);
    void* blkptr = (void*) kmalloc(size, GFP_KERNEL);

    KASSERT(blk && blkptr);

    blk->index = index;
    blk->ptr = blkptr;
    blk->size = size;
    blk->next = pool->blocks;

    pool->blocks = blk;
    pool->cachesize += 1;

    cachesize += size;

    spinlock_init(&blk->lock);
    spinlock_lock(&blk->lock);

    kprintf(LOG "cache: alloc_block() for %d (%d/%d kB)\n", (int) index, size / 1024, (int) (cachesize / 1024));
    return blkptr;
}


size_t kcache_free(int mode) {
    kcache_pool_t* p = NULL, 
                 *fb = NULL;

    uint64_t __cs = cachesize;

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

            if(unlikely(!fb))
                break;

            kcache_free_pool(fb);
            } break;
    }
    

    kprintf(LOG "cache: freed %d kB of memory (%d)\n", (__cs - cachesize) / 1024, mode);
    return (size_t) __cs - cachesize;
}


void kcache_free_pool(kcache_pool_t* pool) {
    kcache_block_t* blk;
    for(; pool->blocks; ) {
        blk = pool->blocks;
        pool->blocks = blk->next;

        spinlock_lock(&blk->lock);

        kfree(blk->ptr);
        cachesize -= blk->size;

        spinlock_unlock(&blk->lock);
        kfree(blk);
    }

    pool->last_access = timer_getticks();
    pool->cachesize = 0;
}

void kcache_register_pool(kcache_pool_t* pool) {
    memset(pool, 0, sizeof(kcache_pool_t));

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

        cachesize -= blk->size;

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


int kcache_obtain_block(kcache_pool_t* pool, kcache_index_t index, void** ptr, size_t size) {
    kcache_block_t* blk;
    for(blk = pool->blocks; blk; blk = blk->next)
        if(unlikely(blk->index == index))
            break;

    pool->last_access = timer_getticks();

    if(unlikely(!blk)) {
        if(likely(ptr))
            *ptr = __kcache_alloc_block(pool, index, size);

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


mm_state_t* kcache_state() {
    static mm_state_t m;
    m.used = cachesize;
    m.total = kvm_state()->total / 2;
    m.frames = NULL;

    return &m;
}


EXPORT(kcache_free);
EXPORT(kcache_free_pool);
EXPORT(kcache_register_pool);
EXPORT(kcache_unregister_pool);
EXPORT(kcache_free_block);
EXPORT(kcache_obtain_block);
EXPORT(kcache_release_block);
EXPORT(kcache_state);

#endif