#ifdef __rpi__

#include <aplus.h>
#include <aplus/task.h>
#include "rpi.h"

extern task_t* current_task;
extern task_t* kernel_task;
extern list_t* task_queue;

extern uint32_t* kernel_vmm;
extern uint32_t* current_vmm;

extern inode_t* vfs_root;


task_t* task_fork() {
	return NULL;
}


int task_init() {
	extern uint32_t kernel_stack;
	
	kernel_task = current_task = (task_t*) kmalloc(sizeof(task_t));
	memset(current_task, 0, sizeof(task_t));
	
	
	current_task->context.env = (task_env_t*) 0;
	current_task->context.cr3 = (uint32_t) kernel_vmm;
	current_task->context.stack = (uint32_t) &kernel_stack;
	
	
	current_task->pid = schedule_nextpid();
	current_task->cwd = (inode_t*) vfs_root;
	current_task->uid = (uid_t) 0;
	current_task->gid = (gid_t) 0;
	
	current_task->state = TASK_STATE_ALIVE;
	current_task->priority = TASK_PRIORITY_REGULAR;
	current_task->parent = NULL;

	current_task->image = (task_image_t*) kmalloc(sizeof(task_image_t));
	memset(current_task->image, 0, sizeof(task_image_t));

	current_task->image->refcount = 1;


	exec_init_kernel_task(current_task);


	list_add(task_queue, (listval_t) current_task);
	task_switch(current_task);
}


void task_switch_ack() {
	return;
}


void task_switch(task_t* newtask) {
	current_task = newtask;
}


task_t* task_clone(void* entry, void* arg, void* stack, int flags) {
	return NULL;
}

#endif
