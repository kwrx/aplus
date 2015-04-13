#ifdef __rpi__

#include <aplus.h>
#include <arch/rpi/rpi.h>

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

#define _I(s)			\
	__attribute__((interrupt (s)))

#define SWI_RETURN(x)				\
	__asm__ __volatile__ (			\
		"mov r0, %0		\n"			\
		"mov r1, %1		\n"			\
		: : "r"(x), "r"(errno)		\
	); return
		

struct intr_t {
	volatile uint32_t irq_basic_pending;
	volatile uint32_t irq_pending_1;
	volatile uint32_t irq_pending_2;
	volatile uint32_t fiq_control;
	volatile uint32_t irq_enable_1;
	volatile uint32_t irq_enable_2;
	volatile uint32_t irq_enable_basic;
	volatile uint32_t irq_disable_1;
	volatile uint32_t irq_disable_2;
	volatile uint32_t irq_disable_basic;
} * intr = (struct intr_t*) INTR_BASE;

struct intr_timer_t {
	volatile uint32_t load;
	volatile uint32_t value;
	volatile uint32_t control;
	volatile uint32_t irq_clear;
	volatile uint32_t irq_raw;
	volatile uint32_t irq_mask;
	volatile uint32_t reload;
	volatile uint32_t predivider;
	volatile uint32_t running_counter;
} * intr_timer = (struct intr_timer_t*) INTR_TIMER_BASE;




void _I("UNDEF") i_undef() {
	panic("ARM Undefined istruction");
}

void _I("ABORT") i_abrtp() {
	panic("ARM abort prefetch");
}

void _I("ABORT") i_abrtd() {
	panic("ARM abort data");
}


void _I("SWI") i_swint(int r0, int r1, int r2, int r3) {
	register int idx;
	__asm__ (
		"ldr %0, [lr, #-4]"
		: "=r"(idx)
	);


	idx = syscall_invoke(idx & 0xFFFF, r0, r1, r2, r3);
	SWI_RETURN(idx);
}


void _I("IRQ") i_irq() {
	return;
}


void _I("FIQ") i_fiq() {
	intr_timer->irq_clear = 1;

	schedule();
}




int intr_init() {

	intr->irq_enable_basic |= INTR_TIMER;
	intr_timer->load = 1000;
	intr_timer->control = 	INTR_TIMER_CTRL_23BIT 			|
							INTR_TIMER_CTRL_ENABLE			|
							INTR_TIMER_CTRL_INT_ENABLE		|
							INTR_TIMER_CTRL_PRESCALE_1;


	cpsr_set(cpsr_get() & ~(1 << 7));
	cpsr_set(cpsr_get() & ~(1 << 6));


	return 0;
}

#endif
