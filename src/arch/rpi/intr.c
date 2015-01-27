#ifdef __rpi__

#include <aplus.h>
#include "rpi.h"

#include <stdint.h>
#include <stddef.h>

static void intr_set(uint32_t i, void* hwnd) {
	mmio_w32(i << 2, 0xEA000000 | (((uint32_t) hwnd - (8 + (4 * i))) >> 2));
}


__attribute__((interrupt ("UNDEF")))
static void i_undef() {
	for(;;);
}


__attribute__((interrupt ("ABORT")))
static void i_abrtp() {
	for(;;);
}

__attribute__((interrupt ("ABORT")))
static void i_abrtd() {
	for(;;);
}

__attribute__((interrupt ("SWI")))
static void i_swint() {
	kprintf("SWI OCCURRED\n");
	for(;;);
}

__attribute__((interrupt ("IRQ")))
static void i_irq() {
	return;
}

//__attribute__((interrupt ("FIQ")))
//static void i_fiq() {
	
//}




int intr_init() {
	//intr_set(0, &i_reset);
	intr_set(1, &i_undef);
	intr_set(2, &i_swint);
	intr_set(3, &i_abrtp);
	intr_set(4, &i_abrtd);
	/* 5: reserved */
	intr_set(6, &i_irq);
	//intr_set(7, &i_fiq);


	cpsr_set(cpsr_get() & ~(1 << 7));
	cpsr_set(cpsr_get() & ~(1 << 6));

	__asm__ ("swi #4");
	return 0;
}

#endif
