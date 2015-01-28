#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

#include <stdint.h>
#include <stddef.h>

#define _I(s)		__attribute__((interrupt (s)))


void i_reset() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

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
void i_swint(int r0, int r1, int r2, int r3) {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

_I("IRQ")
void i_irq() {
	kprintf("intr: %s\n", __func__);
	for(;;);
}

//_I("FIQ")
void i_fiq() {
	kprintf("intr: %s\n", __func__);
	for(;;);	
}




int intr_init() {

	extern uint32_t* interrupt_vector_table;

	int i;
	for(i = 0; i < 7; i++)
		mmio_w32(i << 2, interrupt_vector_table[i]);	


	cpsr_set(cpsr_get() & ~(1 << 7));
	cpsr_set(cpsr_get() & ~(1 << 6));


	return 0;
}

#endif
