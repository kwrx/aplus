

extern int intr_init();
extern int timer_init();
extern int task_init();

void i386_init() {
	(void) intr_init();
	(void) timer_init();
	(void) task_init();
}
