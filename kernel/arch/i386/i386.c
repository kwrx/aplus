

extern int intr_init();
extern int timer_init();
extern int task_init();
extern int pci_init();

void i386_init() {
	__builtin_cpu_init();

	(void) intr_init();
	(void) pci_init();
	(void) timer_init();
	(void) task_init();
}
