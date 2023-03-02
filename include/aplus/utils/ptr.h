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
                                                                        
#ifndef _APLUS_UTILS_PTR_H
#define _APLUS_UTILS_PTR_H

#ifndef __ASSEMBLY__


#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/ipc.h>
#include <aplus/utils/hashmap.h>
#include <stdint.h>


#define SHARED_PTR_MAGIC            0xEEADC0DE



__BEGIN_DECLS


#define shared_ptr(type)                \
    struct {                            \
        uint32_t magic;                 \
        uint32_t refcount;              \
        spinlock_t lock;                \
        __typeof__(type) data;          \
    }*

#define shared_ptr_new(type, gfp)                                                                   \
    ({                                                                                              \
        struct {                                                                                    \
            uint32_t magic;                                                                         \
            uint32_t refcount;                                                                      \
            spinlock_t lock;                                                                        \
            __typeof__(type) data;                                                                  \
        } s = {                                                                                     \
            .magic = SHARED_PTR_MAGIC,                                                              \
            .refcount = 1,                                                                          \
            .lock = SPINLOCK_INIT_WITH_FLAGS(SPINLOCK_FLAGS_RECURSIVE),                             \
        };                                                                                          \
        __typeof__(s)* p = (__typeof__(s)*) kcalloc(1, sizeof(s) + sizeof(__typeof__(type)), gfp);  \
        memcpy(p, &s, sizeof(s));                                                                   \
        (void*) p;                                                                                  \
    })


#define shared_ptr_ref(ptr)                                                     \
    ({                                                                          \
        DEBUG_ASSERT((ptr));                                                    \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        __atomic_add_fetch(&(ptr)->refcount, 1, __ATOMIC_SEQ_CST);              \
        (ptr);                                                                  \
    })

#define shared_ptr_dup(ptr, gfp)                                                \
    ({                                                                          \
        DEBUG_ASSERT((ptr));                                                    \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        __typeof__(ptr) __dup = shared_ptr_new(__typeof__((ptr)->data), gfp);   \
        memcpy(&__dup->data, &ptr->data, sizeof(__typeof__((ptr)->data)));      \
        __dup;                                                                  \
    })

#define shared_ptr_unshare(ptr, gfp)                                            \
    ({                                                                          \
        DEBUG_ASSERT((ptr));                                                    \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        __typeof__(ptr) __dup = shared_ptr_new(__typeof__((ptr)->data), gfp);   \
        memcpy(&__dup->data, &ptr->data, sizeof(__typeof__((ptr)->data)));      \
        shared_ptr_free(ptr);                                                   \
        __dup;                                                                  \
    })


#define shared_ptr_free(ptr)                                                    \
    {                                                                           \
        DEBUG_ASSERT((ptr));                                                    \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        if(__atomic_sub_fetch(&(ptr)->refcount, 1, __ATOMIC_SEQ_CST) == 0) {    \
            kfree((ptr));                                                       \
        }                                                                       \
    }


#define shared_ptr_access(ptr, var, fn)                                         \
    {                                                                           \
        DEBUG_ASSERT((ptr));                                                    \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        void __shared_ptr_cleanup(__typeof__((ptr))* __ptr) {                   \
            DEBUG_ASSERT((*__ptr));                                             \
            DEBUG_ASSERT((*__ptr)->magic == SHARED_PTR_MAGIC);                  \
            spinlock_unlock(&(*__ptr)->lock);                                   \
            shared_ptr_free(*__ptr);                                            \
        }                                                                       \
        spinlock_lock(&(ptr)->lock);                                            \
        __scoped(__shared_ptr_cleanup)                                          \
        __typeof__((ptr)) __ref = shared_ptr_ref(ptr);                          \
        __typeof__((ptr)->data)* var = &__ref->data;                            \
        { fn; };                                                                \
    }

#define shared_ptr_nullable_access(ptr, var, fn)                                \
    if(likely((ptr) != NULL)) {                                                 \
        void __shared_ptr_cleanup(__typeof__((ptr))* __ptr) {                   \
            DEBUG_ASSERT((*__ptr));                                             \
            DEBUG_ASSERT((*__ptr)->magic == SHARED_PTR_MAGIC);                  \
            spinlock_unlock(&(*__ptr)->lock);                                   \
            shared_ptr_free(*__ptr);                                            \
        }                                                                       \
        DEBUG_ASSERT((ptr)->magic == SHARED_PTR_MAGIC);                         \
        spinlock_lock(&(ptr)->lock);                                            \
        __scoped(__shared_ptr_cleanup)                                          \
        __typeof__((ptr)) __ref = shared_ptr_ref(ptr);                          \
        __typeof__((ptr)->data)* var = &__ref->data;                            \
        { fn; };                                                                \
    }


__END_DECLS

#endif
#endif
