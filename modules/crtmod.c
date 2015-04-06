
extern int init();
extern int dnit();
extern int sched_yield();

int __init() {
	//atexit(dnit);
	if(init() != 0)
		return -1;

	for(;;)
		sched_yield();
}
