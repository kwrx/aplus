#ifdef __rpi__

#include <aplus.h>
#include <aplus/task.h>
#include "rpi.h"

task_t* current_task = NULL;
task_t* kernel_task = NULL;


task_t* task_fork() {
	return NULL;
}


int task_init() {
	return 0;
}


void task_switch_ack() {
	return;
}


void task_switch(task_t* newtask) {
	return;
}


task_t* task_clone(void* entry, void* arg, void* stack, int flags) {
	return NULL;
}

#endif
