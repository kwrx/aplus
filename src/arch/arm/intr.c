#include <xdev.h>
#include <xdev/debug.h>
#include <libc.h>


#ifdef __GNUC__
#define _I(s)			\
	__attribute__((interrupt (s)))
#else
#define _I(s)
#endif


void _I("UNDEF") i_undef() {
	kprintf(ERROR, "PANIC! ARM Undefined istruction\n");
}

void _I("ABORT") i_abrtp() {
	kprintf(ERROR, "PANIC! ARM abort prefetch\n");
}

void _I("ABORT") i_abrtd() {
	kprintf(ERROR, "PANIC! ARM abort data\n");
}

void _I("SWI") i_swint(int r0, int r1, int r2, int r3) {
	register int idx;
	__asm__ (
		"ldr %0, [lr, #-4]"
		: "=r"(idx)
	);


	idx = syscall_invoke(idx & 0xFFFF, r0, r1, r2, r3);
	
	__asm__ (
		"mov r0, %0;"
		"mov r1, %1;"
		: : "r"(idx), "r"(errno)
	);
}

void _I("IRQ") i_irq() {
	return;
}

void _I("FIQ") i_fiq() {
	return;
}
