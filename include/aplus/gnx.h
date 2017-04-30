#ifndef _GNX_H
#define _GNX_H


#define GNX_DYNLIB_PATH "libgnx.so"


typedef struct {
    void (*Initialize) (void);
    void (*Dispose) (void);

    void* handle;
} gnx_t;


#ifdef GNX_DYNLIB
#include <sys/types.h>
#include <dlfcn.h>

#   define __SYM(x, y)                                      \
        dlsym(y, #x)
#   define __GNX_DYNLIB_LOAD(x)                             \
    x->handle = dlopen(GNX_DYNLIB_PATH, RTLD_NOW);          \
    if(!x->handle) {                                        \
        perror(GNX_DYNLIB_PATH);                            \
        break;                                              \
    }
#   define __GNX_DYNLIB_FREE(x)                             \
        dlclose(x->handle)
#else
#   define __SYM(x, y) x
#   define __GNX_DYNLIB_LOAD(x) (void) 0
#   define __GNX_DYNLIB_FREE(x) (void) 0


#   ifdef __cplusplus
extern "C" {
#   endif

void __gnx_Initialize(void);
void __gnx_Dispose(void);

#   ifdef __cplusplus
}
#   endif
#endif




#define GnxLoadLibrary(x)                                                       \
        gnx_t* x = NULL;                                                        \
        do {                                                                    \
            x = (gnx_t*) calloc(1, sizeof(gnx_t));                              \
            if(!(x)) {                                                          \
                perror("GnxLoadLibrary");                                       \
                break;                                                          \
            }                                                                   \
                                                                                \
            __GNX_DYNLIB_LOAD((x));                                             \
            (x)->Initialize = __SYM(__gnx_Initialize, (x)->handle);             \
            (x)->Dispose = __SYM(__gnx_Dispose, (x)->handle);                   \
        } while(0)

#define GnxUnloadLibrary(x)                                                     \
    do {                                                                        \
        __GNX_DYNLIB_FREE(x);                                                   \
        free(x);                                                                \
    } while(0)



#endif