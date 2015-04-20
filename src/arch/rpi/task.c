#ifdef __rpi__

#include <aplus.h>
#include <aplus/task.h>
#include <arch/rpi/rpi.h>

extern task_t* current_task;
extern task_t* kernel_task;
extern list_t* task_queue;

extern uint32_t* kernel_vmm;
extern uint32_t* current_vmm;

extern inode_t* vfs_root;


task_t* task_fork() {
	return NULL;
}


void task_switch_ack() {
	
}


void task_switch(task_t* newtask) {
	task_t* old = current_task;
	current_task = newtask;


	vmm_switch(current_task->context.cr3);
	task_switch_ack();
	task_context_switch(&old->context.env, &current_task->context.env);
}

int task_init() {
	extern uint32_t kernel_stack;
	
	kernel_task = current_task = (task_t*) kmalloc(sizeof(task_t));
	memset(current_task, 0, sizeof(task_t));
	
	
	current_task->context.env = (task_env_t*) 0;
	current_task->context.cr3 = (uint32_t) kernel_vmm;
	current_task->context.stack = (uint32_t) &kernel_stack;
	
	
	current_task->pid = schedule_nextpid();
	current_task->cwd = vfs_root;
	current_task->root = vfs_root;
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

	return 0;
}


task_t* task_clone(void* entry, void* arg, void* stack, int flags) {
	task_t* child = (task_t*) kmalloc(sizeof(task_t));
	memset(child, 0, sizeof(task_t));


	child->pid = schedule_nextpid();
	child->uid = current_task->uid;
	child->gid = current_task->gid;
	
	child->state = TASK_STATE_ALIVE;
	child->priority = current_task->priority;
	child->clock = 0;

	child->root = current_task->root;
	child->cwd = current_task->cwd;
	child->exe = current_task->exe;



	if(flags & CLONE_FILES) {
		for(int i = 0; i < TASK_MAX_FD; i++)
			child->fd[i] = current_task->fd[i];
	}


	if(flags & CLONE_PARENT)
		child->parent = current_task->parent;	
	else
		child->parent = current_task;

 	if(flags & CLONE_SIGHAND) {
		child->signal_handler = current_task->signal_handler;
		child->signal_sig = current_task->signal_sig;
	}


	if(flags & CLONE_VM) {
		child->context.cr3 = current_task->context.cr3;
		child->image = current_task->image;
	} else {
		panic("Cloning virtual memory not supported yet");
#if 0
		child->image = (task_image_t*) kmalloc(sizeof(task_image_t));
		memset(child->image, 0, sizeof(task_image_t));

		child->context.cr3 = vmm_create();
		vmm_mapkernel(child->context.cr3);


		if(current_task->image->vaddr && current_task->image->length) {
			void* addr = (void*) kvmalloc(current_task->image->length);
			memcpy(addr, (void*) current_task->image->vaddr, current_task->image->length);

			vmm_map(child->context.cr3, mm_paddr(addr), current_task->image->vaddr, current_task->image->length);
		}
#endif
	}

	child->image->vaddr = current_task->image->vaddr;
	child->image->length = current_task->image->length;
	child->image->ptr = current_task->image->ptr;
	child->image->refcount++;


	if(stack == NULL)
		stack = (void*) kvmalloc(TASK_STACKSIZE);

	memset(stack, 0, TASK_STACKSIZE);
	stack = (void*) ((int) stack + TASK_STACKSIZE);


	
	child->context.stack = (uint32_t) stack - TASK_STACKSIZE;
	child->context.env = (task_env_t*) ((uint32_t) stack - sizeof(task_env_t));
	
	child->context.env->r0 = (uint32_t) arg;
	child->context.env->r14 = (uint32_t) entry;
	child->context.env->r13 = (uint32_t) child->context.env; 
	
	
	list_add(task_queue, (listval_t) child);
	return child;
}

#endif
