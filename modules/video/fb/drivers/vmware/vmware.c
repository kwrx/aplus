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

#include "lib/svga.h"

extern fbdev_t* fbdev;

#if defined(__i386__) || defined(__x86_64__)


int vmware_getvideomode(fbdev_mode_t* m) {
    m->width = gSVGA.width;
    m->height = gSVGA.height;
    m->bpp = gSVGA.bpp;
    m->vx =
    m->vy = 0;
    m->lfbptr = gSVGA.fbMem;
    
    return E_OK;
}

int vmware_setvideomode(fbdev_mode_t* m) {
    SVGA_SetMode(m->width, m->height, m->bpp);

    return E_OK;
}



int vmware_init(void) {
    if(SVGA_Init() != E_OK)
        return E_ERR;

    #define ALIGN(x)                                        \
        ((((uintptr_t) x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
    
    if(gSVGA.fbMem) {
        uintptr_t frame = ALIGN(gSVGA.fbMem) - PAGE_SIZE;
        uintptr_t end = ALIGN(frame + gSVGA.fbSize);

        for(; frame < end; frame += PAGE_SIZE)
            map_page(frame, frame, 1);
    } else
        return E_ERR;
    


    fbdev->name = "VMWare SVGA-II";
    fbdev->setvideomode = vmware_setvideomode;
    fbdev->getvideomode = vmware_getvideomode;
    
    return E_OK;
#else
int bga_init(void) {
    return E_ERR;
#endif
}
