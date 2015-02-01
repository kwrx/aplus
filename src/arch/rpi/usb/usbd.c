#ifdef __rpi__

#include <aplus.h>
#include "../rpi.h"

#if HAVE_USB
#include <usbd/usbd.h>
#include <types.h>
#endif

int usbd_init() {
#if HAVE_USB
	register int e;
	#define check(x)														\
		e = x;																\
		if(unlikely(e != OK)) {												\
			kprintf("usb: %s return this error code -%d\n", #x, -e);		\
			return -1;														\
		}

	check(UsbInitialise());
	UsbCheckForChange();

	/* TODO */
#endif
	return 0;
}

#endif
