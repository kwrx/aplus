#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

MODULE_NAME("pc/video/bga");
MODULE_DEPS("arch/x86");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");

#if defined(__i386__)
#include <arch/i386/i386.h>
#include <arch/i386/pci.h>
#include <arch/i386/pci_list.h>
#elif defined(__x86_64__)
#include <arch/x86_64/x86_64.h>
#endif

#include <aplus/fb.h>

#define VBE_DISPI_IOPORT_INDEX              0x01CE
#define VBE_DISPI_IOPORT_DATA               0x01CF
#define VBE_DISPI_INDEX_ID                  0x0000
#define VBE_DISPI_INDEX_XRES                0x0001
#define VBE_DISPI_INDEX_YRES                0x0002
#define VBE_DISPI_INDEX_BPP                 0x0003
#define VBE_DISPI_INDEX_ENABLE              0x0004
#define VBE_DISPI_INDEX_BANK                0x0005
#define VBE_DISPI_INDEX_VIRT_WIDTH          0x0006
#define VBE_DISPI_INDEX_VIRT_HEIGHT         0x0007
#define VBE_DISPI_INDEX_X_OFFSET            0x0008
#define VBE_DISPI_INDEX_Y_OFFSET            0x0009
#define VBE_DISPI_INDEX_VBOX_VIDEO          0x000A
#define VBE_DISPI_INDEX_FB_BASE_HI          0x000B

#define VBE_DISPI_DISABLED                  0x00
#define VBE_DISPI_ENABLED                   0x01
#define VBE_DISPI_GETCAPS                   0x02
#define VBE_DISPI_8BIT_DAC                  0x20
#define VBE_DISPI_LFB_ENABLED               0x40
#define VBE_DISPI_NOCLEARMEM                0x80

#define CHECK_BGA(n)                        (n >= 0xB0C0 || n <= 0xB0C5)
#define BGA_VIDEORAM_SIZE                   0x1000000

static fbdev_t fbdev;
static uintptr_t __lfbptr = 0;


#if 0
int bga_getvideomode(fbdev_mode_t* m) {
    
    #define rd(i, v)                                        \
        outw(VBE_DISPI_IOPORT_INDEX, i);                    \
        (v) = inw(VBE_DISPI_IOPORT_DATA);
        
    
    rd(VBE_DISPI_INDEX_XRES, m->width);
    rd(VBE_DISPI_INDEX_YRES, m->height);
    rd(VBE_DISPI_INDEX_BPP, m->bpp);
    rd(VBE_DISPI_INDEX_X_OFFSET, m->vx);
    rd(VBE_DISPI_INDEX_Y_OFFSET, m->vy);
    
    m->lfbptr = (void*) __lfbptr;
    memcpy(&fbmode, m, sizeof(fbdev_mode_t));
    
    return E_OK;
}

int bga_setvideomode(fbdev_mode_t* m) {


    #define wr(i, v)                                        \
        outw(VBE_DISPI_IOPORT_INDEX, i);                    \
        outw(VBE_DISPI_IOPORT_DATA, v);


    wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    wr(VBE_DISPI_INDEX_XRES, m->width);
    wr(VBE_DISPI_INDEX_YRES, m->height);
    wr(VBE_DISPI_INDEX_BPP, m->bpp);
    wr(VBE_DISPI_INDEX_X_OFFSET, m->vx);
    wr(VBE_DISPI_INDEX_Y_OFFSET, m->vy);
    wr(VBE_DISPI_INDEX_VIRT_WIDTH, m->width + m->vx);
    wr(VBE_DISPI_INDEX_VIRT_HEIGHT, m->height + m->vy);

    m->lfbptr = (void*) __lfbptr;
    memcpy(&fbmode, m, sizeof(fbdev_mode_t));

    wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    fbdev->enabled = 1;
    return E_OK;
}


int bga_update_surface(fbdev_surface_t* surface) {
    uintptr_t stride = fbmode.bpp << 3;
    uintptr_t p = __lfbptr + ((surface->y * fbmode.width * stride) + surface->x * stride);
    uintptr_t k = (uintptr_t) surface->ptr;
    uintptr_t h = surface->height;
    
    register int i, j;
    for(i = p, j = k; h--; i += stride, j += surface->stride)
        memcpy((void*) p, (void*) j, surface->stride);
        
    return 0;
}

#endif


int init(void) {

    outw(VBE_DISPI_IOPORT_INDEX, 0);
    
    int n = inw(VBE_DISPI_IOPORT_DATA);
    if(!(CHECK_BGA(n)))
        return E_ERR;


    __lfbptr = 0;

    void pci_func(uint32_t device, uint16_t vendor_id, uint16_t device_id, void* arg) {
        
        if(likely(!(
            ((vendor_id == 0x1234) || (vendor_id == 0x80EE)) &&
            ((device_id == 0x1111) || (device_id == 0xBEEF))
        ))) return;
        
        __lfbptr = (uintptr_t) pci_read_field(device, PCI_BAR0, 4);
    }

    int i;
    for(i = 0; i < 65536 && !__lfbptr; i++)
        pci_scan(&pci_func, i, NULL);

    if(!__lfbptr)
        return E_ERR;


    #define ALIGN(x)                                        \
        (((x) + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1))
    
    if(__lfbptr) {
        uintptr_t frame = ALIGN(__lfbptr) - PAGE_SIZE;
        uintptr_t end = ALIGN(frame + BGA_VIDEORAM_SIZE);

        for(; frame < end; frame += PAGE_SIZE)
            map_page(frame, frame, 1);
    } else
        return E_ERR;
    

    memset(&fbdev, 0, sizeof(fbdev));
    fbdev.init = NULL;
    fbdev.dnit = NULL;

    if(unlikely(fbdev_register_device(&fbdev, "Bochs VBE Extensions") != E_OK))
        return E_ERR;
    
    return E_OK;
}


int dnit(void) {
    fbdev_unregister_device(&fbdev);
    return E_OK;
}