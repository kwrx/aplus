#include <aplus/base.h>
#include <stdlib.h>
#include <string.h>


void* (*__libaplus_malloc) (size_t) = malloc;
void* (*__libaplus_calloc) (size_t, size_t) = calloc;
void (*__libaplus_free) (void*) = free;


int libaplus_init(void* (*mallocfp) (size_t), void* (*callocfp) (size_t, size_t), void (*freefp) (void*)) {
    __libaplus_malloc = mallocfp;
    __libaplus_calloc = callocfp;
    __libaplus_free = freefp;

    return 0;
}