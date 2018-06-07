#include <sys/mman.h>

void* mmap(void* addr, size_t len, int prot, int flags, int fd, off_t offset) {
    uintptr_t p[6];
    p[0] = addr;
    p[1] = len;
    p[2] = prot;
    p[3] = flags;
    p[4] = fd;
    p[5] = offset;
    return __mmap(p);
}