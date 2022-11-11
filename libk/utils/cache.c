/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2019 Antonino Natale
 * 
 * 
 * This file is part of aplus.
 * 
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/memory.h>
#include <aplus/errno.h>
#include <stdint.h>
#include <fcntl.h>

#include <aplus/utils/cache.h>
#include <aplus/utils/hashmap.h>


static int __cache_key_cmp(void* const* a, void* const* b) {
    return (int) ((uintptr_t) a - (uintptr_t) b);
}

static size_t __cache_hash_default(void* const* data) {

    size_t hash = 0;

    const uint8_t *byte = (const uint8_t *) &data;

    for (size_t i = 0; i < sizeof(void*); ++i) {
        hash += *byte++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);

    return hash;

}


void cache_init(cache_t* cache, cache_ops_t* ops, void* userdata) {
    
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(ops->load);
    DEBUG_ASSERT(ops->sync);


    memset(cache, 0, sizeof(cache_t));
    memcpy(&cache->ops, ops, sizeof(cache_ops_t));

    cache->size = 0;
    cache->userdata = userdata;

    hashmap_init(&cache->map, __cache_hash_default, __cache_key_cmp);


    spinlock_init(&cache->lock);

}


void cache_destroy(cache_t* cache) {

    DEBUG_ASSERT(cache);

    __lock(&cache->lock, {
        
        if(cache->ops.sync) {

            cache_key_t key;
            cache_value_t value;

            hashmap_foreach(key, value, &cache->map) {
                cache->ops.sync(cache, cache->userdata, key, value);
            }

        }

        hashmap_cleanup(&cache->map);

        cache->size = 0;
        cache->userdata = NULL;

    });

}


void cache_sync_all(cache_t* cache) {

    DEBUG_ASSERT(cache);


    cache_key_t key;

    hashmap_foreach_key(key, &cache->map) {
        cache_sync(cache, key);
    }

}


__returns_nonnull
cache_value_t __cache_get(cache_t* cache, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(key);

    cache_value_t value = NULL;

    __lock(&cache->lock, {

        value = hashmap_get(&cache->map, key);

        if(value == NULL) {

            if(cache->ops.load) {
                value = cache->ops.load(cache, cache->userdata, key);
            }

            if(value != NULL) {

                hashmap_put(&cache->map, key, value);
                cache->size++;

            }

        }

    });


    PANIC_ON(value != NULL && "cache: failed to load key");

    return value;

}


__returns_nonnull
cache_value_t __cache_sync(cache_t* cache, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(key);


    cache_value_t value = NULL;

    __lock(&cache->lock, {

        cache_value_t v = hashmap_get(&cache->map, key);

        if(v != NULL) {

            if(cache->ops.sync) {
                value = cache->ops.sync(cache, cache->userdata, key, value);
            }

            if(v != value) {

                hashmap_remove(&cache->map, key);
                hashmap_put(&cache->map, key, value);
    
            }

            value = v;

        }

    });


    PANIC_ON(value != NULL && "cache: failed to sync key");

    return value;

}


__returns_nonnull
cache_value_t __cache_remove(cache_t* cache, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(key);


    cache_value_t value = NULL;

    __lock(&cache->lock, {

        if((value = hashmap_remove(&cache->map, key)) != NULL) {
            cache->size--;
        }

    });


    PANIC_ON(value != NULL && "cache: failed to remove key");

    return value;

}
