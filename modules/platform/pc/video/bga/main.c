#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/task.h>
#include <libc.h>

MODULE_NAME("pc/video/bga");
MODULE_DEPS("arch/x86,video/fb");
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
#define BGA_ID                              "Bochs VBE Extensions"


static fbdev_t fbdev;



int bga_update(fbdev_t* dev) {
    
    #define wr(i, v)                                        \
        outw(VBE_DISPI_IOPORT_INDEX, i);                    \
        outw(VBE_DISPI_IOPORT_DATA, v);


    wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    wr(VBE_DISPI_INDEX_XRES, dev->vs.xres);
    wr(VBE_DISPI_INDEX_YRES, dev->vs.yres);
    wr(VBE_DISPI_INDEX_BPP, dev->vs.bits_per_pixel);
    wr(VBE_DISPI_INDEX_X_OFFSET, dev->vs.xoffset);
    wr(VBE_DISPI_INDEX_Y_OFFSET, dev->vs.yoffset);
    wr(VBE_DISPI_INDEX_VIRT_WIDTH, dev->vs.xres_virtual);
    wr(VBE_DISPI_INDEX_VIRT_HEIGHT, dev->vs.yres_virtual);

    if((dev->vs.activate & FB_ACTIVATE_MASK) == 0)
        wr(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
    

    static int rgba[] = {
        0, 0, 0, 0, 0, 0, 0, 0,     /* NULL             */
        3, 5, 3, 2, 2, 0, 0, 0,     /* 8bpp RRRGGGBB    */
        5, 11, 6, 6, 5, 0, 0, 0,    /* 16bpp RGB565     */
        8, 16, 8, 8, 8, 0, 0, 0,    /* 24bpp RGB24      */
        8, 16, 8, 8, 8, 0, 8, 24,   /* 32bpp ARGB       */
    };

    dev->vs.red.length = rgba[dev->vs.bits_per_pixel + 0];
    dev->vs.red.offset = rgba[dev->vs.bits_per_pixel + 1];
    dev->vs.green.length = rgba[dev->vs.bits_per_pixel + 2];
    dev->vs.green.offset = rgba[dev->vs.bits_per_pixel + 3];
    dev->vs.blue.length = rgba[dev->vs.bits_per_pixel + 4];
    dev->vs.blue.offset = rgba[dev->vs.bits_per_pixel + 5];
    dev->vs.transp.length = rgba[dev->vs.bits_per_pixel + 6];
    dev->vs.transp.offset = rgba[dev->vs.bits_per_pixel + 7];


    strncpy(dev->fs.id, BGA_ID, 16);
    dev->fs.smem_start = (unsigned long) dev->userdata;
    dev->fs.smem_len = BGA_VIDEORAM_SIZE;
    dev->fs.type = FB_TYPE_PLANES;
    dev->fs.type_aux = 0;
    dev->fs.visual = FB_VISUAL_TRUECOLOR;
    dev->fs.xpanstep =
    dev->fs.ypanstep =
    dev->fs.ywrapstep = 0;
    dev->fs.line_length = dev->vs.xres_virtual * (dev->vs.bits_per_pixel / 8);
    dev->fs.mmio_start =
    dev->fs.mmio_len = 0;
    dev->fs.accel = 0;
    dev->fs.capabilities = 0;

    return E_OK;
}

int init(void) {

    outw(VBE_DISPI_IOPORT_INDEX, 0);
    
    int n = inw(VBE_DISPI_IOPORT_DATA);
    if(!(CHECK_BGA(n)))
        return E_ERR;


    uintptr_t __lfbptr = 0;

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
    fbdev.update = bga_update;
    fbdev.userdata = (void*) __lfbptr;

    if(unlikely(fbdev_register_device(&fbdev, BGA_ID) != E_OK))
        return E_ERR;
    
    return E_OK;
}


int dnit(void) {
    fbdev_unregister_device(&fbdev);
    return E_OK;
}