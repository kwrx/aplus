#include <stdint.h>
#include <stddef.h>
#include <aplus/core/base.h>
#include <aplus/core/debug.h>




__attribute__((malloc))
void* malloc(size_t size) {

    static uint8_t staticpool[32 * 1024]; // 32Kib
    static uint32_t offset = 0;


    DEBUG_ASSERT(size);
    DEBUG_ASSERT(offset < sizeof(staticpool));

    void* p = &staticpool[offset];
    offset += size;
    return p;

}


__attribute__((malloc))
void* calloc(size_t n, size_t m) {

    void* p;
    if(unlikely(!(p = malloc(n * m))))
        return NULL;

    return p;

}


void free(void* p) {
    (void) p;
}