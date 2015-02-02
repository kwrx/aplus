#ifdef __rpi__

#include <aplus.h>
#include <stdint.h>

static void* __rpi_atags = NULL;
static int __rpi_armtype = 0;


int arch_init() {
	serial_init();

	rpi_parse_atags(__rpi_armtype, __rpi_atags);

	mm_init();
	lfb_init();
	intr_init();
	syscall_init();

#if HAVE_USB
	usbd_init();
#endif

	return 0;
}


void go_usermode() {
	return;
}


void cpu_idle() {
	__asm__ __volatile__ ("nop");
}

void cpu_wait() {
	__asm__ __volatile__ ("nop");
}


void rpi_save_args(int unused, int armtype, void* atags) {
	__rpi_atags = atags;
	__rpi_armtype = armtype;
}


#endif
