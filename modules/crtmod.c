
extern int init();
extern int dnit();
extern int sched_yield();
extern void _exit(int);

void _start() {
	//atexit(dnit);
	if(init() != 0)
		_exit(1);

	for(;;)
		sched_yield();
}
