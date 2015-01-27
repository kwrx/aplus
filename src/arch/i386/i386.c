#ifdef __i386__

#include <aplus.h>
#include "i386.h"


int arch_init() {
	serial_init();
	mm_init();
	desc_init();
	syscall_init();
	pci_init();

	return 0;
}


void cpu_idle() {
	__asm__ ("pause; hlt;");
}

void cpu_wait() {
	__asm__ ("pause");
}

#endif
