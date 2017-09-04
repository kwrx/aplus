/* config.h.  Generated from config.h.in by configure.  */
#ifndef _CONFIG_H
#define _CONFIG_H

#define HAVE_STDDEF_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1

#define HAVE_STDLIB_H 1
#define HAVE_STDIO_H 1
#define HAVE_STRING_H 1
#define HAVE_MATH_H 1
#define HAVE_FCNTL_H 1
#define HAVE_UNISTD_H 1
#define HAVE_ERRNO_H 1
#define HAVE_SCHED_H 1
#define HAVE_SYS_TYPES_H 1



#define HAVE_ZIP_H 1
#define HAVE_FFI_H 1
/* #undef HAVE_GC_H */

//#define DEBUG 1
#define CONFIG_SYSROOT ""


#define APP_NAME "avm"
#define APP_VERSION "0.1-alpha"
#define APP_COPY "2016 Antonino Natale"
#define APP_CC "gcc"



#define APP_CDATE        __DATE__
#define APP_CTIME        __TIME__

#define APP_CC_VERSION        __VERSION__



#define APP_VERSION_FORMAT                                        \
        "%s %s\nCopyright (C) %s\nBuilt with %s %s (%s:%s)\n"
#define APP_VERSION_ARGS                                        \
        APP_NAME, APP_VERSION, APP_COPY, APP_CC, APP_CC_VERSION,                \
        APP_CDATE, APP_CTIME



#if HAVE_STDDEF_H
#include <stddef.h>
#else
#error "stddef.h not found (freestanding header)"
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#else
#error "stdarg.h not found (freestanding header)"
#endif

#if HAVE_STDINT_H
#include <stdint.h>
#else
#error "stdint.h not found (freestanding header)"
#endif


#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#else
typedef int pid_t;
typedef int off_t;
typedef int ssize_t;
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
void* calloc(size_t, size_t);
void free(void*);
void abort(void);
void exit(int);
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#else
int printf(const char*, ...);
#endif

#if HAVE_MATH_H
#include <math.h>
#else
double fmod(double, double);
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#define O_RDONLY        0

#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2
#endif


#if HAVE_ZIP_H
#define CONFIG_JAR        1
#include <zip.h>
#endif

#if HAVE_FFI_H
#define CONFIG_JNI        1
#include <ffi.h>
#endif

#if HAVE_GC_H
#include <gc.h>

static inline void* __gc_calloc(size_t x, size_t y) {
    void* p = GC_malloc_uncollectable(x * y);
    if(!p)
        return NULL;

    register int i = x * y;
    char* d;
    for(d = p; i--; d++)
        *d = 0;
        
    return p;
}

#define strdup(s) GC_STRDUP(s)
#define __calloc __gc_calloc
#define __free GC_FREE
#else
#define __calloc calloc
#define __free free
#endif


#if HAVE_STRING_H
#include <string.h>
#else
void* memset(void* s1, int value, size_t size);
void* memcpy(void* s1, const void* s2, size_t size);
char* strcpy(char* s1, const char* s2);
size_t strlen(const char* s);
char* strdup(const char* s);
char* strcat(char*, const char*);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);
double fmod(double x, double y);
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#else
extern int errno;
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#else

int close(int);
off_t lseek(int, off_t, int);
ssize_t read(int, void*, size_t);
int open(const char*, int, ...);
pid_t getpid();
#endif

#if HAVE_SCHED_H
#include <sched.h>
#else
int sched_yield(void);
#endif





#if DEBUG
#define LOG(x)            \
    { avm->printf("%s: %s\n", APP_NAME, x); }
#define LOGF(x, y...)    \
    { avm->printf("%s: " x "\n", APP_NAME, y); }

#define ASSERT(x)    \
    { if(unlikely(!(x))) { LOGF("Assertion \"%s\" failed in %s:%d", #x, __FILE__, __LINE__); abort(); } }

#else
#define LOG(x)
#define LOGF(x, y...)
#define ASSERT(x)
#endif

#define PRINTF(x, y...)    \
    { avm->printf(x, y); }



#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wnonnull"
#endif



#define INITIALIZE_PATH() {                                    \
        avm_config_path_add(CONFIG_SYSROOT "/usr/lib/avm");                \
        avm_config_path_add(CONFIG_SYSROOT "/usr/local/lib/avm");            \
        avm_config_path_add(CONFIG_SYSROOT "/usr/share/java");                \
    }

#endif
