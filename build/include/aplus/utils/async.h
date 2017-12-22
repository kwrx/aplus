#ifndef _APLUS_ASYNC_H
#define _APLUS_ASYNC_H

#ifndef __GNUC__
#error "compile with -gnu99 extension"
#endif

#include <pthread.h>




typedef pthread_t async_t;


#define async(x, y) ({                                              \
                                                                    \
    pthread_t th;                                                   \
    if(pthread_create(&th, NULL,                                    \
        ({ void* __fn__ (void* arg) { x };                          \
            __fn__; }), (void*) y) != 0)                            \
        return -1;                                                  \
                                                                    \
    th;                                                             \
})


#define await(x, y)                                                 \
    pthread_join(x, y)



#define async_do(x, y)                                              \
    async(for(;;) { x }, y)

#define async_do_while(x, cond, y)                                  \
    async(do { x } while(cond);, y)  

#define async_timer(x, y, us)                                       \
    async(for(;; usleep(us)) { x }, y)

#endif