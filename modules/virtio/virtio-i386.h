#if defined(__i386__)
#include <arch/i386/i386.h>

uint8_t virtio_r8(uint16_t p) {
    return inb(p);
}

uint16_t virtio_r16(uint16_t p) {
    return inw(p);
}

uint32_t virtio_r32(uint16_t p) {
    return inl(p);
}

void virtio_w8(uint16_t p, uint8_t v) {
    outb(p, v);
}

void virtio_w16(uint16_t p, uint16_t v) {
    outw(p, v);
}

void virtio_w32(uint16_t p, uint32_t v) {
    outl(p, v);
}

#endif