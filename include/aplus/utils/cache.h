/*                                                                      
 * Author(s):                                                           
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
                                                                        
#ifndef _APLUS_UTILS_CACHE_H
#define _APLUS_UTILS_CACHE_H

#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/utils/hashmap.h>
#include <stdint.h>


typedef void* cache_value_t;
typedef void* cache_key_t;
typedef struct cache cache_t;

typedef cache_value_t (*cache_fetch_handler_t)(cache_t*, void*, cache_key_t);
typedef void (*cache_commit_handler_t)(cache_t*, void*, cache_key_t, cache_value_t);
typedef void (*cache_release_handler_t)(cache_t*, void*, cache_key_t, cache_value_t);


typedef struct cache_ops {

    cache_fetch_handler_t fetch;
    cache_commit_handler_t commit;
    cache_release_handler_t release;

} cache_ops_t;

typedef struct cache {

    HASHMAP(cache_key_t, cache_value_t) map;

    size_t size;
    size_t capacity;

    void* userdata;

    cache_ops_t ops;
    spinlock_t lock;

} cache_t;


__BEGIN_DECLS

void cache_init(cache_t* cache, cache_ops_t* ops, size_t capacity, void* userdata);
void cache_destroy(cache_t* cache);
void cache_commit_all(cache_t* cache);

__returns_nonnull
cache_value_t __cache_get(cache_t* cache, cache_key_t key);

void __cache_commit(cache_t* cache, cache_key_t key);
void __cache_remove(cache_t* cache, cache_key_t key);


#define cache_get(cache, key)                                           \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_get((cache), (cache_key_t) ((uintptr_t) (__key)));      \
    })

#define cache_commit(cache, key)                                        \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_commit((cache), (cache_key_t) ((uintptr_t) (__key)));   \
    })

#define cache_remove(cache, key)                                        \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_remove((cache), (cache_key_t) ((uintptr_t) (__key)));   \
    })

__END_DECLS

#endif
#endif
