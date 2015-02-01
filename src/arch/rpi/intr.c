#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

#include <stdint.h>
#include <stddef.h>

#define _I(s)		__attribute__((interrupt (s)))



_I("UNDEF")
void i_undef() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}


_I("ABORT")
void i_abrtp() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

_I("ABORT")
void i_abrtd() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

_I("SWI")
void i_swint(int r3, int r2, int r1, int r0) {
	kprintf("intr: %s (%d, %d, %d, %d)\n", __func__, r0, r1, r2, r3);
	return 10;
}

_I("IRQ")
void i_irq() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

_I("")
void i_fiq() {
	kprintf("intr: %s\n", __func__);
	for(;;);	
}




int intr_init() {
	cpsr_set(cpsr_get() & ~(1 << 7));
	cpsr_set(cpsr_get() & ~(1 << 6));

	__asm__("swi 0x0" : : "r"(1000), "r"(2000), "r"(3000), "r"(4000));
	return 0;
}

#endif
