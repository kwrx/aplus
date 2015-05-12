#ifdef __i386__

#include <aplus.h>
#include <arch/i386/i386.h>


int arch_pre_init() {

	serial_init();
	mm_init();
	desc_init();
	syscall_init();
	pci_init();

	return 0;
}

int arch_post_init() {
#if HAVE_USB
	//usbd_init();
#endif

	return 0;
}



void cpu_halt() {
	__asm__ ("cli; hlt;");
}

void cpu_idle() {
	__asm__ ("pause; hlt;");
}

void cpu_wait() {
	__asm__ ("pause");
}


EXPORT_SYMBOL(cpu_halt);
EXPORT_SYMBOL(cpu_idle);
EXPORT_SYMBOL(cpu_wait);

#endif
