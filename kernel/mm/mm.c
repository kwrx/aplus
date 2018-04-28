#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <aplus/mm.h>

#define ALIGN(x)        (((x) + (PAGE_ALIGN - 1)) & ~(PAGE_ALIGN - 1))

int mm_init(void) {
    pmm_init();
    slab_init();

    return 0;
}
