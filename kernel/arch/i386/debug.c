#include <aplus.h>
#include <aplus/ipc.h>
#include <aplus/debug.h>
#include <arch/i386/i386.h>
#include <libc.h>

#if DEBUG
spinlock_t lck_debug = SPINLOCK_UNLOCKED;


void debug_send(char value) {

	spinlock_lock(&lck_debug);
	static int initialized = 0;

	if(!initialized) {
		initialized = 1;

#if CONFIG_SERIAL_DEBUG
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x80);
		outb(0x3F8 + 0, 0x03);
		outb(0x3F8 + 1, 0x00);
		outb(0x3F8 + 3, 0x03);
		outb(0x3F8 + 2, 0xC7);
		outb(0x3F8 + 4, 0x0B);
#endif	
	}


#if CONFIG_BOCHS
	outb(0xE9, value);
#endif

#if CONFIG_SERIAL_DEBUG
	int i;
	for(i = 0; i < 100000 && ((inb(0x3F8 + 5) & 0x20) == 0); i++)
		;
	outb(0x3F8, value);
#endif
	

	spinlock_unlock(&lck_debug);
}

#endif
