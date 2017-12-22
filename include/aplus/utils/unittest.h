#ifndef _APLUS_UNITTEST_H
#define _APLUS_UNITTEST_H

#include <aplus/base.h>
#include <stdio.h>
#include <time.h>
#include <sys/times.h>
#include <errno.h>


#if defined(TEST) || defined(__unittest__)
#define __unittest_begin()                                                                          \
    {struct {                                                                                       \
        FILE* fp;                                                                                   \
        int errors;                                                                                 \
        int success;                                                                                \
        clock_t t0;                                                                                 \
        clock_t t1;                                                                                 \
    } __unittest;                                                                                   \
                                                                                                    \
    __unittest.fp = fopen("/dev/log", "w");                                                         \
    __unittest.errors = __unittest.success = 0;                                                     \
    if(!__unittest.fp)                                                                              \
        __unittest.fp = stderr;                                                                     \
    fprintf(__unittest.fp, "\n-- Unit Test - %s --\n\n", __FILE__);                                 \
    __unittest.t0 = clock()                                                                         \

#define __unittest_end()                                                                            \
    __unittest.t1 = clock();                                                                        \
    fprintf(__unittest.fp, "\n-- Unit Test - %s --\n", __FILE__);                                   \
    fprintf(__unittest.fp, "-- Errors/Success: %d/%d, Time: %.3fs\n\n",                             \
                __unittest.errors, __unittest.success,                                              \
                (double) (__unittest.t1 - __unittest.t0) / (double) CLOCKS_PER_SEC);                \
    fclose(__unittest.fp);}

#define __unittest(fn, cond, val, ret)                                                              \
    ({                                                                                              \
        ret __r;                                                                                    \
        if(unlikely(!((__r = (fn)) cond (val)))) {                                                  \
            fprintf(__unittest.fp, "\e[31m[ FAIL ] %4d | \'%s\' => %p (%d), errno: %d\e[39m\n",     \
                __LINE__, #fn " " #cond " " #val, __r, __r, errno);                                 \
            __unittest.errors++;                                                                    \
        } else {                                                                                    \
            fprintf(__unittest.fp, "\e[32m[  OK  ] %4d | \'%s\' => %p (%d)\e[39m\n",                \
                __LINE__, #fn " " #cond " " #val, __r, __r);                                        \
            __unittest.success++;                                                                   \
        }                                                                                           \
                                                                                                    \
        __r;                                                                                        \
    })


#define __unittest_p(fn, cond, val, param)                                                          \
    {                                                                                               \
        fn;                                                                                         \
        if(unlikely(!((param) cond (val))))                                                         \
            fprintf(__unittest_fp, "\e[31m[ FAIL ] %4d | \'%s\' => %p (%d), errno: %d\e[39m\n",     \
                __LINE__, #fn " " #cond " " #val, __r, __r, errno);                                 \
        else                                                                                        \
            fprintf(__unittest_fp, "\e[32m[  OK  ] %4d | \'%s\' => %p (%d)\e[39m\n",                \
                __LINE__, #fn " " #cond " " #val, __r, __r);                                        \
    }

    
#else

#define __unittest_begin()                  ((void) 0)
#define __unittest_end()                    ((void) 0)

#define __unittest(fn, cond, val, ret)              \
    fn

#define __unittest_p(fn, cond, val, param)          \
    fn

#endif


#define __unittest_eq(fn, val, ret)         __unittest(fn, ==, val, ret)
#define __unittest_neq(fn, val, ret)        __unittest(fn, !=, val, ret)
#endif