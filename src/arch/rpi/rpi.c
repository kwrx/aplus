#ifdef __rpi__

#include <aplus.h>
#include <stdint.h>

static void* __rpi_atags = NULL;
static int __rpi_armtype = 0;


int arch_pre_init() {
	serial_init();

	rpi_parse_atags(__rpi_armtype, __rpi_atags);

	mm_init();
	intr_init();
	lfb_init();
	syscall_init();

	return 0;
}

int arch_post_init() {
#if HAVE_USB
	usbd_init();
#endif

	return 0;
}


void go_usermode() {
	return;
}


void cpu_halt() {
	for(;;);
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


EXPORT_SYMBOL(cpu_halt);
EXPORT_SYMBOL(cpu_idle);
EXPORT_SYMBOL(cpu_wait);

#endif
