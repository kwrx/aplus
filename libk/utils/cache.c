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


void cache_init(cache_t* cache, cache_ops_t* ops, size_t capacity, void* userdata) {
    
    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(ops);
    DEBUG_ASSERT(ops->fetch);
    DEBUG_ASSERT(ops->commit);
    DEBUG_ASSERT(ops->release);


    memset(cache, 0, sizeof(cache_t));
    memcpy(&cache->ops, ops, sizeof(cache_ops_t));

    cache->size = 0;
    cache->capacity = capacity;
    cache->userdata = userdata;

    hashmap_init(&cache->map, __cache_hash_default, __cache_key_cmp);


    spinlock_init(&cache->lock);

}


void cache_destroy(cache_t* cache) {

    DEBUG_ASSERT(cache);

    __lock(&cache->lock, {
        
        if(likely(cache->ops.commit)) {

            cache_key_t key;
            cache_value_t value;

            hashmap_foreach(key, value, &cache->map) {
                cache->ops.commit(cache, cache->userdata, key, value);
            }

        }

        if(likely(cache->ops.release)) {

            cache_key_t key;
            cache_value_t value;

            hashmap_foreach(key, value, &cache->map) {
                cache->ops.release(cache, cache->userdata, key, value);
            }

        }

        hashmap_cleanup(&cache->map);

        cache->size = 0;
        cache->userdata = NULL;

    });

}


void cache_commit_all(cache_t* cache) {

    DEBUG_ASSERT(cache);


    cache_key_t key;

    hashmap_foreach_key(key, &cache->map) {
        cache_commit(cache, key);
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

            if(cache->ops.fetch) {
                value = cache->ops.fetch(cache, cache->userdata, key);
            }

            if(value != NULL && cache->capacity > 0) {

                if(cache->size < cache->capacity) {

                    hashmap_put(&cache->map, key, value);

                } else {

                    cache_key_t k;
                    cache_value_t v;

                    hashmap_foreach(k, v, &cache->map) {

                        hashmap_remove(&cache->map, k);

                        if(cache->ops.commit) {
                            cache->ops.commit(cache, cache->userdata, k, v);
                        }

                        if(cache->ops.release) {
                            cache->ops.release(cache, cache->userdata, k, v);
                        }

                        cache->size--;

                        break;

                    }

                    hashmap_put(&cache->map, key, value);

                }

                cache->size++;

            }

        }

    });


    PANIC_ASSERT(value != NULL && "cache: failed to fetch key");

    return value;

}


void __cache_commit(cache_t* cache, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(key);


    cache_value_t value = NULL;

    __lock(&cache->lock, {

        cache_value_t v = hashmap_get(&cache->map, key);

        if(v != NULL) {

            if(cache->ops.commit) {
                cache->ops.commit(cache, cache->userdata, key, value);
            }

        }

    });


    PANIC_ASSERT(value != NULL && "cache: failed to commit key");

}


void __cache_remove(cache_t* cache, cache_key_t key) {

    DEBUG_ASSERT(cache);
    DEBUG_ASSERT(key);


    cache_value_t value = NULL;

    __lock(&cache->lock, {

        if((value = hashmap_remove(&cache->map, key)) != NULL) {
            cache->size--;
        }

        if(cache->ops.release) {
            cache->ops.release(cache, cache->userdata, key, value);
        }

    });


    PANIC_ASSERT(value != NULL && "cache: failed to remove key");

}
