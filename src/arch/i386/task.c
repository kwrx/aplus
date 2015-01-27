#ifdef __i386__

#include <aplus.h>
#include <aplus/spinlock.h>
#include <aplus/task.h>
#include <aplus/mm.h>
#include <aplus/list.h>
#include <aplus/fs.h>

#include <setjmp.h>
#include "i386.h"

#if HAVE_SSE
#define SSE_ALIGN(x)	(((uint32_t) x & 0x10) + 0x10)
#endif


#define __FORK			0x80000000


__asm__ (
	".global task_context_switch		\n"
	"task_context_switch:				\n"
	"	cli								\n"
	"	push ebp						\n"
	"	mov ebp, esp					\n"
	"	pushf							\n"
	"	push edx						\n"
	"	push eax						\n"	
	"	push ecx						\n"
	"	push ebx						\n"
	"	push esi						\n"
	"	push edi						\n"
	"	mov eax, [ebp + 8]				\n"
	"	mov edx, [ebp + 12]				\n"
	"	mov [eax], esp					\n"
	"	mov esp, [edx]					\n"
	"	pop edi							\n"
	"	pop esi							\n"
	"	pop ebx							\n"
	"	pop ecx							\n"
	"	pop eax							\n"
	"	pop edx							\n"
	"	popf							\n"
	"	pop ebp							\n"
	"	sti								\n"
	"ret								\n"
);


extern task_t* current_task;
extern task_t* kernel_task;
extern list_t* task_queue;

extern uint32_t* kernel_vmm;
extern uint32_t* current_vmm;

extern volatile inode_t* vfs_root;
extern void task_context_switch(task_env_t** old, task_env_t** new);


void task_switch_ack() {
	outb(0x20, 0x20);
}



task_t* task_clone(void* entry, void* arg, void* stack, int flags) {
	
	task_t* child = (task_t*) kmalloc(sizeof(task_t));
	memset(child, 0, sizeof(task_t));

	child->pid = schedule_nextpid();
	child->exe = current_task->exe;
	child->uid = current_task->uid;
	child->gid = current_task->gid;
	
	child->state = TASK_STATE_ALIVE;
	child->priority = current_task->priority;
	child->clock = 0;


	if(flags & CLONE_FILES) {
		for(int i = 0; i < TASK_MAX_FD; i++)
			child->fd[i] = current_task->fd[i];
	}

	if(flags & CLONE_FS)
		child->cwd = current_task->cwd;
	else
		child->cwd = (inode_t*) vfs_root;

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

		child->image = (task_image_t*) kmalloc(sizeof(task_image_t));
		memset(child->image, 0, sizeof(task_image_t));

		child->context.cr3 = vmm_create();
		vmm_mapkernel(child->context.cr3);


		if(current_task->image->vaddr && current_task->image->length) {
			void* addr = (void*) kvmalloc(current_task->image->length);
			memcpy(addr, (void*) current_task->image->vaddr, current_task->image->length);

			vmm_map(child->context.cr3, mm_paddr(addr), current_task->image->vaddr, current_task->image->length);
		}
	}

	child->image->vaddr = current_task->image->vaddr;
	child->image->length = current_task->image->length;
	child->image->ptr = current_task->image->ptr;
	child->image->refcount++;


	if(stack == NULL)
		stack = (void*) kvmalloc(TASK_STACKSIZE);

	memset(stack, 0, TASK_STACKSIZE);
	stack = (void*) ((int) stack + TASK_STACKSIZE);


	if(flags & __FORK) {
	
		/* Update context */
		schedule_yield();

		entry = (void*) read_eip();
		if(child == current_task)
			return child;

		child->context.stack = (uint32_t) stack - TASK_STACKSIZE;
		child->context.env = (task_env_t*) ((((uint32_t) stack - TASK_STACKSIZE) & ~0xFFF) + ((uint32_t) current_task->context.env & 0xFFF)); 

		memcpy((void*) child->context.stack, (void*) current_task->context.stack, TASK_STACKSIZE);
		memcpy((void*) child->context.env, (void*) current_task->context.env, sizeof(task_env_t));

		int i = 0;
		for(uint32_t* s = (uint32_t*) child->context.stack; i < TASK_STACKSIZE; s++, i += sizeof(uint32_t))
			if((*s & ~0xFFF) == current_task->context.stack)
				*s = ((uint32_t) child->context.stack & ~0xFFF) + (*s & 0xFFF);

		child->context.env->eax = (uint32_t) arg;
		child->context.env->eip = (uint32_t) entry;
		child->context.env->ebp = (uint32_t) child->context.env; 

	} else {
		child->context.stack = (uint32_t) stack - TASK_STACKSIZE;
		child->context.env = (task_env_t*) ((uint32_t) stack - sizeof(task_env_t));
	
		child->context.env->eax = (uint32_t) arg;
		child->context.env->eip = (uint32_t) entry;
		child->context.env->ebp = (uint32_t) child->context.env; 
	}
	

	list_add(task_queue, (listval_t) child);
	return child;
}


void task_switch(task_t* newtask) {

	__asm__("cli");

	task_t* old = current_task;
	current_task = newtask;

#ifdef SCHED_TIMING_DEBUG
	if(current_task->timing_tm != sys_time(NULL)) {
		current_task->timing_tm = sys_time(NULL);

		kprintf("task: [%d] %d %%\n", current_task->pid, (current_task->clock - current_task->timing_last_clock) / CLOCKS_PER_SEC * 100);
		current_task->timing_last_clock = current_task->clock;
	}
#endif

	vmm_switch(current_task->context.cr3);
	
#if HAVE_SSE
	__asm__ ("fxsave [eax]" : : "a"(SSE_ALIGN(old->context.fpu)));
	__asm__ ("fxrstor [eax]" : : "a"(SSE_ALIGN(current_task->context.fpu)));
#endif

	task_switch_ack();
	task_context_switch(&old->context.env, &current_task->context.env);
}






task_t* task_fork() {
	if(unlikely(!current_task))
		return NULL;


	task_t* child = task_clone(NULL, NULL, NULL, CLONE_FILES | CLONE_FS | CLONE_SIGHAND | __FORK);
	if(child == current_task)
		return NULL;

	return child;
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


	return 0;
}


#endif
