/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 * 
 * Copyright (c) 2013-2018 Antonino Natale
 * 
 * 
 * This file is part of aPlus.
 * 
 * aPlus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * aPlus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with aPlus.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef _APLUS_ASYNC_H
#define _APLUS_ASYNC_H

#ifndef __GNUC__
#error "compile with -gnu99 extension"
#endif

#include <pthread.h>


typedef pthread_t async_t;


#define async(x, y) ({                                              \
    pthread_t th;                                                   \
    if(pthread_create(&th, NULL,                                    \
        ({                                                          \
            void* __fn__ (void* __arg) {                            \
                typeof(y) arg = (typeof(y)) __arg;                  \
                x;                                                  \
            };                                                      \
            __fn__;                                                 \
        }), (void*) y) != 0)                                        \
        return -1;                                                  \
                                                                    \
    th;                                                             \
})


#define await(x) ({                                                 \
    void* r;                                                        \
    pthread_join(x, &r);                                            \
    r;                                                              \
})


#define async_do(x, y)                                              \
    async(for(;;) { x }, y)

#define async_do_while(x, cond, y)                                  \
    async(do { x } while(cond);, y)  

#define async_timer(x, y, us)                                       \
    async(for(;; sleep(us)) { x }, y)

#define aexit(e)                                                    \
    pthread_exit((void*) e)

#define ayield()                                                    \
    pthread_yield()


#endif