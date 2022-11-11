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

typedef cache_value_t (*cache_load_handler_t)(cache_t*, void*, cache_key_t);
typedef cache_value_t (*cache_sync_handler_t)(cache_t*, void*, cache_key_t, cache_value_t);

typedef struct cache_ops {

    cache_load_handler_t load;
    cache_sync_handler_t sync;

} cache_ops_t;

typedef struct cache {

    HASHMAP(cache_key_t, cache_value_t) map;

    size_t size;
    void* userdata;

    cache_ops_t ops;
    spinlock_t lock;

} cache_t;


__BEGIN_DECLS

void cache_init(cache_t* cache, cache_ops_t* ops, void* userdata);
void cache_destroy(cache_t* cache);
void cache_sync_all(cache_t* cache);

__returns_nonnull
cache_value_t __cache_get(cache_t* cache, cache_key_t key);

__returns_nonnull
cache_value_t __cache_sync(cache_t* cache, cache_key_t key);

__returns_nonnull
cache_value_t __cache_remove(cache_t* cache, cache_key_t key);


#define cache_get(cache, key)                                           \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_get((cache), (cache_key_t) ((uintptr_t) (__key)));      \
    })

#define cache_sync(cache, key)                                          \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_sync((cache), (cache_key_t) ((uintptr_t) (__key)));     \
    })

#define cache_remove(cache, key)                                        \
    ({                                                                  \
        typeof(key) __key = (key);                                      \
        __cache_remove((cache), (cache_key_t) ((uintptr_t) (__key)));   \
    })

__END_DECLS

#endif
#endif
