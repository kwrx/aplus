#ifndef _APLUS_DEBUG_H
#define _APLUS_DEBUG_H

#ifndef __ASSEMBLY__


#if defined(DEBUG)
#define DEBUG_ASSERT(i)     \
        if(unlikely(!(i)))  \
            kpanicf("ERROR! Assert failed on %s() in %s:%d: '%s'\n", \
                __func__, __FILE__, __LINE__, #i)

#else
#define DEBUG_ASSERT(i)     \
        (void) 0
#endif


#define BUG_ON(i)           \
    if(unlikely(!(i)))      \
        kpanicf("BUG! Found a bug on %s() in %s:%d: '%s'\n", \
            __func__, __FILE__, __LINE__, #i)



__BEGIN_DECLS

void kprintf(const char*, ...);
void kprintf_pause();
void kprintf_resume();
void kprintf_disable();

__attribute__((noreturn))
void kpanicf(const char*, ...);

__END_DECLS

#endif

#endif