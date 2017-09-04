#include <aplus.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/mm.h>
#include <aplus/debug.h>
#include <libc.h>

#include <aplus/fbdev.h>

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#include <arch/i386/pci_list.h>
#elif defined(__x86_64__)
#include <arch/x86_64/x86_64.h>
#endif



#define VESA_VIDEORAM_SIZE                0x1000000


extern fbdev_t* fbdev;
static fbdev_mode_t fbmode;
static uintptr_t __lfbptr = 0;

#if defined(__i386__) || defined(__x86_64__)


int vesa_getvideomode(fbdev_mode_t* m) {
    memcpy(&fbmode, m, sizeof(fbdev_mode_t));
    
    return E_OK;
}

int vesa_setvideomode(fbdev_mode_t* m) {
    memcpy(&fbmode, m, sizeof(fbdev_mode_t));
    fbdev->enabled = 1;
    
    return E_OK;
}


int vesa_update_surface(fbdev_surface_t* surface) {
    uintptr_t stride = fbmode.bpp << 3;
    uintptr_t p = __lfbptr + ((surface->y * fbmode.width * stride) + surface->x * stride);
    uintptr_t k = (uintptr_t) surface->ptr;
    uintptr_t h = surface->height;
    
    register int i, j;
    for(i = p, j = k; h--; i += stride, j += surface->stride)
        memcpy((void*) p, (void*) j, surface->stride);
        
    return 0;
}

int vesa_init(void) {

    __lfbptr = 0xE0000000;

    #define ALIGN(x)                                        \
        (((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
    

    uintptr_t frame = ALIGN(__lfbptr) - PAGE_SIZE;
    uintptr_t end = ALIGN(frame + VESA_VIDEORAM_SIZE);

    for(; frame < end; frame += PAGE_SIZE)
        map_page(frame, frame, 1);
    


    fbdev->name = "VBE - Vesa Graphics Extension 3.0";
    fbdev->setvideomode = vesa_setvideomode;
    fbdev->getvideomode = vesa_getvideomode;
    fbdev->update_surface = vesa_update_surface;
    
    
    return E_OK;
#else
int vesa_init(void) {
    return E_ERR;
#endif
}