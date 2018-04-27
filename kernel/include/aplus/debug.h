#ifndef _DEBUG_H
#define _DEBUG_H

#include <aplus.h>
#include <libc.h>



#define LOG                     "[ LOG   ] "
#define WARN                    "[ WARN  ] "
#define INFO                    "[ INFO  ] "
#define ERROR                   "[ ERROR ] "
#define USER                    "[ USER  ] "


#ifndef __ASSEMBLY__
void debug_send(char value);
void debug_dump(void* context, char* errmsg, uintptr_t dump, uintptr_t errcode);
void debug_stacktrace(int);


#if !DEBUG
#define debug_send(a)               (void) 0
#define debug_dump(x, y, z, w)      (void) 0
#define debug_stacktrace(x)         (void) 0
#define KASSERT(x)
#define KASSERTF(x, y...)
#define kprintf(a, b...)
#else

int kprintf(const char* fmt, ...);

#define KASSERT(x)                                                                                                                                          \
    if(unlikely(!(x)))                                                                                                                                      \
        {                                                                                                                                                   \
            kprintf(ERROR "%s(): Assertion \"%s\" failed in %s:%d\n", __func__, #x, __FILE__, __LINE__);                                                    \
            debug_stacktrace(5);                                                                                                                            \
            for(;;);                                                                                                                                        \
        }

#define KASSERTF(x, y...)                                                                                                                                   \
    if(unlikely(!(x)))                                                                                                                                      \
        {                                                                                                                                                   \
            kprintf(ERROR "%s(): Assertion \"%s\" failed in %s:%d\nDetails: ", __func__, #x, __FILE__, __LINE__);                                           \
            kprintf(y);                                                                                                                                     \
            kprintf("\n");                                                                                                                                  \
            debug_stacktrace(5);                                                                                                                            \
            for(;;);                                                                                                                                        \
        }


#endif
#endif

#endif
