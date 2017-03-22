#include <aplus.h>
#include <aplus/debug.h>
#include <aplus/module.h>
#include <aplus/vfs.h>
#include <aplus/ipc.h>
#include <aplus/intr.h>
#include <aplus/timer.h>
#include <aplus/mm.h>
#include <aplus/mmio.h>
#include <libc.h>

MODULE_NAME("pc/audio/ac97");
MODULE_DEPS("");
MODULE_AUTHOR("Antonino Natale");
MODULE_LICENSE("GPL");


#if defined(__i386__) || defined(__x86_64__)
#	if defined(__i386__)
#		include <arch/i386/i386.h>
#		include <arch/i386/pci.h>
#	elif defined(__x86_64__)
#		include <arch/x86_64/x86_64.h>
#		include <arch/x86_64/pci.h>
#	endif



static uint32_t ac97_devices[] = {
    0x8086, 0x2417,
    0x8086, 0x2415,
    0x0000, 0x0000
};



static void find_ac97_pci(uint32_t device, uint16_t vendorid, uint16_t deviceid, void* arg) {
	int i;
    for(i = 0; ac97_devices[i]; i += 2)
        if(vendorid == ac97_devices[i] && deviceid == ac97_devices[i + 1])
		    *((uintptr_t*) arg) = device;
}

int init(void) {
    uintptr_t ac97_pci = 0;
    pci_scan(&find_ac97_pci, -1, &ac97_pci);

    if(unlikely(!ac97_pci)) {
        kprintf(ERROR, "ac97: no pci device found!\n");
        return E_ERR;
    }

    vfs_mkdev("snd", -1, S_IFCHR | 0666);
    vfs_mkdev("mixer", -1, S_IFCHR | 0666);
    
    kprintf(WARN, "ac97: Intel AC97 not yet supported! it doesn't work\n");
    return E_OK;
}

#else

int init(void) {
	return E_OK;
}

#endif


int dnit(void) {
	return E_OK;
}